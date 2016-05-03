#include "k/context.h"

#include "etl/array_count.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/types.h"

#include "common/abi_sizes.h"
#include "common/message.h"
#include "common/descriptor.h"

#include "k/memory.h"
#include "k/context_layout.h"
#include "k/object_table.h"
#include "k/panic.h"
#include "k/registers.h"
#include "k/reply_gate.h"
#include "k/reply_sender.h"
#include "k/scheduler.h"

using etl::armv7m::Word;

namespace k {

template struct ObjectSubclassChecks<Context, kabi::context_size>;

static_assert(__builtin_offsetof(Context::Body, save) == 0,
    "Context::Body::save offset is wrong (should be zero, isn't)");

static_assert(K_CONTEXT_BODY_STACK_OFFSET ==
    __builtin_offsetof(Context::Body, save.named.stack),
    "K_CONTEXT_BODY_STACK_OFFSET is wrong");

/*******************************************************************************
 * Context-specific stuff
 */

Context::Context(Generation g, Body & body)
  : Object{g},
    _body(body) {
  body.ctx_item.owner = this;
  body.sender_item.owner = this;
}

void Context::nullify_exchanged_keys(unsigned preserved) {
  // Had to do this somewhere, this is as good a place as any.
  // (The fields in question are private, so this can't be at top level.)
  // (Putting it in the ctor hits the ill-defined non-trivial ctor rules.)
  static_assert(K_CONTEXT_BODY_OFFSET == __builtin_offsetof(Context, _body),
                "K_CONTEXT_BODY_OFFSET is wrong");

  // Right, actual implementation now:
  for (unsigned i = preserved; i < config::n_message_keys; ++i) {
    _body.keys[i] = Key::null();
  }
}

void Context::set_reply_gate(ReplyGate & g) {
  PANIC_IF(g.is_bound(), "ReplyGate double bind");

  _body.reply_gate = &g;
  g.set_owner(this);
}

uint32_t Context::do_ipc(uint32_t stack, Descriptor d) {
  set_stack(stack);

  // Perform first phase of IPC.
  if (d.get_send_enabled()) {
    key(d.get_target()).deliver_from(this);
  } else if (d.get_receive_enabled()) {
    key(d.get_source()).get()->deliver_to(this);
  }
  // Note that if neither bit is set, we'll just return with the registers
  // unchanged.

  do_deferred_switch();

  return current->stack();
}

void Context::do_key_op(uint32_t sysnum, Descriptor d) {
  switch (sysnum) {
    case 1:  // Copy Key
      key(d.get_target()) = key(d.get_source());
      return;

    case 2:  // Discard Keys
      for (unsigned k = d.get_source(); k <= d.get_target(); ++k) {
        key(k) = Key::null();
      }
      return;

    default:
      // Bad sysnum used.  This should probably become a fault?
      return;
  }
}

void Context::complete_receive(BlockingSender * sender) {
  _body.save.sys = sender->on_blocked_delivery(get_message_keys());
}

void Context::complete_receive(Exception e, uint32_t param) {
  _body.save.sys = { Message::failure(e, param), 0 };
  nullify_exchanged_keys();
}

void Context::block_in_receive(List<Context> & list) {
  // TODO should we decide to permit non-blocking recieves... here's the spot.
  _body.ctx_item.unlink();
  list.insert(&_body.ctx_item);
  _body.state = State::receiving;

  pend_switch();
}

void Context::block_in_reply() {
  _body.ctx_item.unlink();
  _body.state = State::receiving;

  pend_switch();
}

void Context::complete_blocked_receive(Brand brand, Sender * sender) {
  runnable.insert(&_body.ctx_item);
  _body.state = State::runnable;
  _body.save.sys.brand = brand;

  pend_switch();

  _body.save.sys.m = sender->on_delivery(get_message_keys());
}

void Context::complete_blocked_receive(Exception e, uint32_t param) {
  runnable.insert(&_body.ctx_item);
  _body.state = State::runnable;

  pend_switch();

  complete_receive(e, param);
}

void Context::apply_to_mpu() {
  using etl::armv7m::mpu;

  for (unsigned i = 0; i < config::n_task_regions; ++i) {
    mpu.write_rnr(i);
    auto object = _body.memory_regions[i].get();
    auto region =
        object->get_region_for_brand(_body.memory_regions[i].get_brand());
    mpu.write_rbar(region.rbar);
    mpu.write_rasr(region.rasr);
  }
}

void Context::make_runnable() {
  switch (_body.state) {
    case State::sending:
      _body.sender_item.unlink();
      on_blocked_delivery_aborted();
      break;

    case State::receiving:
      _body.ctx_item.unlink();
      complete_blocked_receive(Exception::would_block);
      break;

    case State::stopped:
      runnable.insert(&_body.ctx_item);
      _body.state = State::runnable;
      break;

    case State::runnable:
      break;
  }
}


/*******************************************************************************
 * Implementation of Sender and BlockingSender
 */

Priority Context::get_priority() const {
  return _body.priority;
}

Message Context::on_delivery(Keys & k) {
  // We're either synchronously delivering our message, or have been found on a
  // block list and asked to deliver.

  // We assume that the descriptor is unchanged from when we sent.  If it is
  // not, it only affects us, since the recipient has been chosen.
  auto d = get_descriptor();

  // Make a copy of the message we're trying to send, sanitized.  We need to do
  // this because we may be about to receive into the same memory, below.
  auto m = _body.save.sys.m.sanitized();

  k.keys[0] = d.is_call() ? make_reply_key() : key(0);
  for (unsigned ki = 1; ki < config::n_message_keys; ++ki) {
    k.keys[ki] = key(ki);
  }

  // Atomically transition to receive state if requested by the program.
  if (d.get_receive_enabled()) {
    // If we're calling, reuse the reply key we just minted:
    auto & source = d.is_call() ? k.keys[0]
                                : key(d.get_source());
    // And this is where our outgoing message would be overwritten; thus the
    // copy above.
    source.get()->deliver_to(this);
  }

  return m;
}

void Context::block_in_send(Brand brand, List<BlockingSender> & list) {
  PANIC_UNLESS(this == current, "non-current Context block_in_send");

  if (get_descriptor().get_block()) {
    _body.saved_brand = brand;
    list.insert(&_body.sender_item);
    _body.ctx_item.unlink();
    _body.state = State::sending;

    pend_switch();
  } else {
    // Unprivileged code is unwilling to block for delivery.
    _body.save.sys = { Message::failure(Exception::would_block), 0 };
  }
}

ReceivedMessage Context::on_blocked_delivery(Keys & k) {
  runnable.insert(&_body.ctx_item);
  _body.state = State::runnable;

  pend_switch();
  return {
    .m = on_delivery(k),
    .brand = _body.saved_brand,
  };
}

void Context::on_blocked_delivery_aborted() {
  runnable.insert(&_body.ctx_item);
  _body.state = State::runnable;
  pend_switch();

  _body.save.sys = { Message::failure(Exception::would_block), 0 };
}

Key Context::make_reply_key() const {
  if (_body.reply_gate) {
    auto maybe_key = _body.reply_gate.ref()->make_key(0);
    if (maybe_key) return maybe_key.ref();
  }
  return Key::null();
}


/*******************************************************************************
 * Implementation of Context protocol.
 */

void Context::deliver_from(Brand brand, Sender * sender) {
  Keys k;
  Message m = sender->on_delivery(k);

  ScopedReplySender reply_sender{k.keys[0]};

  switch (m.desc.get_selector()) {
    case 0:  // read_register
    case 1:  // write_register
      if (m.d0 >= etl::array_count(_body.save.raw)) {
        reply_sender.message() = Message::failure(Exception::bad_argument);
        return;
      }

      if (m.desc.get_selector() == 0) {  // read
        reply_sender.message().d0 = _body.save.raw[m.d0];
      } else {  // write
        _body.save.raw[m.d0] = m.d1;
      }
      return;

    case 2:  // read_key
    case 3:  // write_key
      {
        auto r = m.d0;

        if (r >= config::n_task_keys) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        if (m.desc.get_selector() == 2) {  // read
          reply_sender.set_key(1, key(r));
        } else {  // write
          key(r) = k.keys[1];
        }
      }
      return;

    case 4:  // read_region
    case 5:  // write_region
      {
        auto n = m.d0;

        if (n >= config::n_task_regions) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        if (m.desc.get_selector() == 4) {  // read
          reply_sender.set_key(1, _body.memory_regions[n]);
        } else {  // write
          _body.memory_regions[n] = k.keys[1];
          if (current == this) apply_to_mpu();
        }
      }
      return;

    case 6:  // make_runnable
      make_runnable();
      pend_switch();
      return;

    case 7:  // read_priority
      reply_sender.message().d0 = _body.priority;
      return;

    case 8:  // write_priority
      {
        auto priority = m.d0;

        if (priority >= config::n_priorities) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        _body.priority = priority;

        if (_body.ctx_item.is_linked()) _body.ctx_item.reinsert();
        if (_body.sender_item.is_linked()) _body.sender_item.reinsert();
      }
      return;

    case 9:  // read_low_registers
    case 10:  // read_high_registers
      {
        auto index = (m.desc.get_selector() == 9) ? 0 : 5;
        reply_sender.message().d0 = _body.save.raw[index];
        reply_sender.message().d1 = _body.save.raw[index + 1];
        reply_sender.message().d2 = _body.save.raw[index + 2];
        reply_sender.message().d3 = _body.save.raw[index + 3];
        reply_sender.message().d4 = _body.save.raw[index + 4];
      }
      return;

    case 11:  // write_low_registers
    case 12:  // write_high_registers
      {
        auto index = (m.desc.get_selector() == 11) ? 0 : 5;
        _body.save.raw[index    ] = reply_sender.message().d0;
        _body.save.raw[index + 1] = reply_sender.message().d1;
        _body.save.raw[index + 2] = reply_sender.message().d2;
        _body.save.raw[index + 3] = reply_sender.message().d3;
        _body.save.raw[index + 4] = reply_sender.message().d4;
      }
      return;

    default:
      reply_sender.message() =
        Message::failure(Exception::bad_operation, m.desc.get_selector());
      return;
  }
}

}  // namespace k
