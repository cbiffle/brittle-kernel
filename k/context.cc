#include "k/context.h"

#include "etl/array_count.h"
#include "etl/prediction.h"

#include "etl/armv7m/mpu.h"
#include "etl/armv7m/types.h"

#include "common/abi_sizes.h"
#include "common/message.h"
#include "common/descriptor.h"
#include "common/selectors.h"
#include "common/sysnums.h"

#include "k/memory.h"
#include "k/context_layout.h"
#include "k/object_table.h"
#include "k/panic.h"
#include "k/registers.h"
#include "k/reply_sender.h"
#include "k/scheduler.h"

using etl::armv7m::Word;

namespace k {

// Check that this meets the head-size and body-size requirements.
template struct ObjectSubclassChecks<Context, kabi::context_size>;

// Check the kernel entry sequence's assumption about the offset of the save
// area.
static_assert(__builtin_offsetof(Context::Body, save) == 0,
    "Context::Body::save offset is wrong (should be zero, isn't)");

// Check the kernel entry sequence's assumption about the offset of the stack
// pointer.
static_assert(K_CONTEXT_BODY_STACK_OFFSET ==
    __builtin_offsetof(Context::Body, save.named.stack),
    "K_CONTEXT_BODY_STACK_OFFSET is wrong");

static constexpr Brand reply_brand_mask = Brand(1) << 63;


/*******************************************************************************
 * Context-specific stuff
 */

Context::Context(Generation g, Body & body)
  : Object{g},
    _body(body) {
  // Had to do this somewhere, this is as good a place as any.
  // (The fields in question are private, so this can't be at top level.)
  static_assert(K_CONTEXT_BODY_OFFSET == __builtin_offsetof(Context, _body),
                "K_CONTEXT_BODY_OFFSET is wrong");

  body.ctx_item.owner = this;
  body.sender_item.owner = this;
  body.expected_reply_brand = reply_brand_mask;
}

KeysRef Context::get_receive_keys() {
  return KeysRef{_body.keys, _body.save.named.r11};
}

KeysRef Context::get_sent_keys() {
  return KeysRef{_body.keys, _body.save.named.r10};
}

uint32_t Context::do_ipc(uint32_t stack, Descriptor d) {
  set_stack(stack);

  // TODO: this is a temporarily paranoid check.
  PANIC_IF(key(0).get()->get_kind() != Kind::null, "kr0 written");

  // Perform first phase of IPC.
  if (d.get_send_enabled()) {
    key(d.get_target()).deliver_from(this);
  } else if (d.get_receive_enabled()) {
    auto & k = key(d.get_source());
    k.get()->deliver_to(k.get_brand(), this);
  }
  // Note that if neither bit is set, we'll just return with the registers
  // unchanged.

  do_deferred_switch();

  return current->stack();
}

void Context::do_key_op(uint32_t sysnum, Descriptor d) {
  switch (sysnum) {
    case kabi::sysnum_copy_key:
      // Only write the register if it is non-zero, protecting kr0.
      if (d.get_target()) key(d.get_target()) = key(d.get_source());
      return;

    default:
      // Bad sysnum used.  This should probably become a fault?
      return;
  }
}

void Context::complete_receive(BlockingSender * sender) {
  _body.save.sys = sender->on_blocked_delivery(get_receive_keys());
}

void Context::complete_receive(Exception e, uint32_t param) {
  _body.save.sys = { Message::failure(e, param), 0 };
  // Leave keys as-is so caller can retry or whatever.
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

void Context::complete_blocked_receive(Brand const & brand, Sender * sender) {
  make_runnable();

  _body.save.sys.m = sender->on_delivery(get_receive_keys());
  _body.save.sys.brand = brand;
}

void Context::complete_blocked_receive(Exception e, uint32_t param) {
  make_runnable();
  complete_receive(e, param);
}

void Context::apply_to_mpu() {
  using etl::armv7m::mpu;

  // Disable MPU to keep half-applied settings from kicking in.
  mpu.write_ctrl(mpu.read_ctrl().with_enable(false));

  for (unsigned i = 0; i < config::n_task_regions; ++i) {
    auto object = memory_region(i).get();
    auto region =
        object->get_region_for_brand(memory_region(i).get_brand());
    mpu.write_rbar(region.rbar.with_valid(true).with_region(i));
    mpu.write_rasr(region.rasr);
  }

  // Re-enable MPU.
  mpu.write_ctrl(mpu.read_ctrl().with_enable(true));
}

void Context::make_runnable() {
  runnable.insert(&_body.ctx_item);
  _body.state = State::runnable;
  pend_switch();
}


/*******************************************************************************
 * Implementation of Sender and BlockingSender
 */

Priority Context::get_priority() const {
  return _body.priority;
}

Message Context::on_delivery(KeysRef k) {
  // We're either synchronously delivering our message, or have been found on a
  // block list and asked to deliver.

  // We assume that the descriptor is unchanged from when we sent.  If it is
  // not, it only affects us, since the recipient has been chosen.
  auto d = get_descriptor();

  // Make a copy of the message we're trying to send, sanitized.  We need to do
  // this because we may be about to receive into the same memory, below.
  auto m = _body.save.sys.m.sanitized();

  auto sent_keys = get_sent_keys();
  auto k0 = d.is_call() ? make_reply_key() : sent_keys.get(0);
  k.set(0, k0);
  for (unsigned ki = 1; ki < config::n_message_keys; ++ki) {
    k.set(ki, sent_keys.get(ki));
  }

  // Atomically transition to receive state if requested by the program.
  if (d.get_receive_enabled()) {
    // If we're calling, reuse the reply key we just minted:
    auto & source = d.is_call() ? k0
                                : key(d.get_source());
    // And this is where our outgoing message would be overwritten; thus the
    // copy above.
    source.get()->deliver_to(source.get_brand(), this);
  }

  return m;
}

void Context::block_in_send(Brand const & brand, List<BlockingSender> & list) {
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

ReceivedMessage Context::on_blocked_delivery(KeysRef k) {
  make_runnable();
  return {
    .m = on_delivery(k),
    .brand = _body.saved_brand,
  };
}

void Context::on_blocked_delivery_aborted() {
  make_runnable();

  _body.save.sys = { Message::failure(Exception::would_block), 0 };
}

Key Context::make_reply_key() {
  auto maybe_key = make_key(_body.expected_reply_brand);
  PANIC_UNLESS(maybe_key, "ctx refused reply key");
  return maybe_key.ref();
}

bool Context::is_reply_brand(Brand const & brand) {
  static_assert(uint32_t(reply_brand_mask) == 0,
      "Here we assume that reply_brand_mask has only top bits set.");
  return (uint32_t(brand >> 32) & uint32_t(reply_brand_mask >> 32));
}

void Context::advance_reply_brand() {
  _body.expected_reply_brand =
    (_body.expected_reply_brand + 1) | reply_brand_mask;
}

void Context::invalidation_hook() {
  _body.ctx_item.unlink();
  _body.sender_item.unlink();
  _body.state = State::stopped;
  advance_reply_brand();

  // Invalidate current Context cache, if needed.
  if (this == current) pend_switch();
}


/*******************************************************************************
 * Implementation of Context protocol.
 */

void Context::deliver_from(Brand const & brand, Sender * sender) {
  // Handle all reply messages, even with the *wrong* reply brand, differently.
  if (ETL_LIKELY(is_reply_brand(brand))) {
    // Fail all messages if the brand is wrong (use of a stale reply key).
    // TODO: this is the approach I've used historically, but should we really
    // *reply* to errant replies?
    if (ETL_UNLIKELY(brand != _body.expected_reply_brand)) {
      Object::deliver_from(brand, sender);
      return;
    }

    // Advance our expected brand, implicitly invalidating the current key.
    advance_reply_brand();

    // Unblocking from awaiting reply is supposed to advance the expected reply
    // brand, which should prevent us from reaching this point.
    PANIC_UNLESS(is_awaiting_reply(), "context not awaiting reply");

    complete_blocked_receive(brand, sender);
  } else {
    // Context service message.
    handle_protocol(brand, sender);
  }
}

void Context::deliver_to(Brand const & brand, Context * ctx) {
  // This detects errant receive from Contexts ... but also receive from our
  // own reply key.  Distinguish the two.
  if (brand != _body.expected_reply_brand || ctx != this) {
    Object::deliver_to(brand, ctx);
    return;
  }

  block_in_reply();
}

void Context::handle_protocol(Brand const &, Sender * sender) {
  Keys k;
  auto m = sender->on_delivery(k);
  ScopedReplySender reply_sender{k.keys[0]};

  namespace S = selector::context;

  switch (m.desc.get_selector()) {
    case S::read_register:
    case S::write_register:
      if (m.d0 >= etl::array_count(_body.save.raw)) {
        reply_sender.message() = Message::failure(Exception::bad_argument);
        return;
      }

      if (m.desc.get_selector() == S::read_register) {
        reply_sender.message().d0 = _body.save.raw[m.d0];
      } else {  // write
        _body.save.raw[m.d0] = m.d1;
      }
      return;

    case S::read_key_register:
    case S::write_key_register:
      {
        auto r = m.d0;
        bool is_read = m.desc.get_selector() == S::read_key_register;

        if (r >= config::n_task_keys || (!is_read && r == 0)) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        if (is_read) {  // read
          reply_sender.set_key(1, key(r));
        } else {  // write
          key(r) = k.keys[1];
        }
      }
      return;

    case S::read_region_register:
    case S::write_region_register:
      {
        auto n = m.d0;

        if (n >= config::n_task_regions) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        if (m.desc.get_selector() == S::read_region_register) {
          reply_sender.set_key(1, _body.memory_regions[n]);
        } else {  // write
          _body.memory_regions[n] = k.keys[1];
          if (current == this) apply_to_mpu();
        }
      }
      return;

    case S::make_runnable:
      switch (_body.state) {
        case State::sending:
          _body.sender_item.unlink();
          on_blocked_delivery_aborted();
          break;

        case State::receiving:
          // Invalidate any outstanding reply keys.
          if (is_awaiting_reply()) advance_reply_brand();
          _body.ctx_item.unlink();
          complete_blocked_receive(Exception::would_block);
          break;

        case State::stopped:
          make_runnable();
          break;

        case State::runnable:
          break;
      }
      return;

    case S::get_priority:
      reply_sender.message().d0 = _body.priority;
      return;

    case S::set_priority:
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

    case S::read_low_registers:
    case S::read_high_registers:
      {
        auto index = (m.desc.get_selector() == S::read_low_registers) ? 0 : 5;
        reply_sender.message().d0 = _body.save.raw[index];
        reply_sender.message().d1 = _body.save.raw[index + 1];
        reply_sender.message().d2 = _body.save.raw[index + 2];
        reply_sender.message().d3 = _body.save.raw[index + 3];
        reply_sender.message().d4 = _body.save.raw[index + 4];
      }
      return;

    case S::write_low_registers:
    case S::write_high_registers:
      {
        auto index = (m.desc.get_selector() == S::write_low_registers) ? 0 : 5;
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
