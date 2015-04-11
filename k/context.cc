#include "k/context.h"

#include "etl/armv7m/instructions.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/types.h"

#include "k/address_range.h"
#include "k/context_layout.h"
#include "k/ipc.h"
#include "k/object_table.h"
#include "k/registers.h"
#include "k/reply_sender.h"
#include "k/unprivileged.h"

using etl::armv7m::Word;

namespace k {

/*******************************************************************************
 * Global objects
 */

Context * current;
List<Context> runnable;
bool switch_pending;

static void do_deferred_switch() {
  if (!switch_pending) return;

  switch_pending = false;

  while (runnable.is_empty()) {
    etl::armv7m::wait_for_interrupt();
  }

  current = runnable.peek().ref()->owner;
}

static void pend_switch() {
  switch_pending = true;
}


/*******************************************************************************
 * Context-specific stuff
 */

Context::Context()
  : _stack{nullptr},
    _keys{},
    _ctx_item{this},
    _sender_item{this},
    _priority{0},
    _state{State::stopped},
    _saved_brand{0} {}

void Context::nullify_exchanged_keys(unsigned preserved) {
  // Had to do this somewhere, this is as good a place as any.
  // (The fields in question are private, so this can't be at top level.)
  // (Putting it in the ctor hits the ill-defined non-trivial ctor rules.)
  static_assert(K_CONTEXT_SAVE_OFFSET == __builtin_offsetof(Context, _save),
                "K_CONTEXT_SAVE_OFFSET is wrong");

  // Right, actual implementation now:
  for (unsigned i = preserved; i < config::n_message_keys; ++i) {
    _keys[i].nullify();
  }
}

Descriptor Context::get_descriptor() const {
  return _save.sys.m.d0;
}

void Context::do_syscall() {
  switch (get_descriptor().get_sysnum()) {
    case 0:  // IPC
      do_ipc();
      break;

    case 1:  // Copy Key
      do_copy_key();
      break;
    
    default:
      do_bad_sys();
      break;
  }
}

void Context::do_ipc() {
  auto d = get_descriptor();

  // Perform first phase of IPC.
  if (d.get_send_enabled()) {
    key(d.get_target()).deliver_from(this);
  } else if (d.get_receive_enabled()) {
    key(d.get_source()).get()->deliver_to(this);
  } else {
    // Simply return with registers unchanged.
    // (Weirdo.)
  }

  do_deferred_switch();
}

void Context::do_copy_key() {
  auto d = get_descriptor();
  key(d.get_target()) = key(d.get_source());
}

void Context::do_bad_sys() {
  put_message(0, Message::failure(Exception::bad_syscall));
}

void Context::complete_receive(BlockingSender * sender) {
  complete_receive_core(sender->get_saved_brand(), sender);
}

void Context::complete_receive_core(Brand brand, Sender * sender) {
  put_message(brand, sender->get_message());

  for (unsigned i = 0; i < config::n_message_keys; ++i) {
    key(i) = sender->get_message_key(i);
  }

  sender->complete_send();
}

void Context::complete_receive(Exception e, uint32_t param) {
  put_message(0, Message::failure(e, param));
  nullify_exchanged_keys();
}

void Context::block_in_receive(List<Context> & list) {
  ETL_ASSERT(this == current);

  // TODO should we decide to permit non-blocking recieves... here's the spot.
  _ctx_item.unlink();
  list.insert(&_ctx_item);
  _state = State::receiving;

  pend_switch();
}

void Context::complete_blocked_receive(Brand brand, Sender * sender) {
  _ctx_item.unlink();  // from the receiver list
  runnable.insert(&_ctx_item);
  _state = State::runnable;

  complete_receive_core(brand, sender);

  pend_switch();
}

void Context::complete_blocked_receive(Exception e, uint32_t param) {
  _ctx_item.unlink();  // from the receiver list
  runnable.insert(&_ctx_item);
  _state = State::runnable;

  complete_receive(e, param);

  pend_switch();
}

void Context::put_message(Brand brand, Message const & m) {
  _save.sys.m = m;
  _save.sys.m.d0 = _save.sys.m.d0.sanitized();
  _save.sys.b = brand;
}

void Context::apply_to_mpu() {
  using etl::armv7m::mpu;

  for (unsigned i = 0; i < config::n_task_regions; ++i) {
    mpu.write_rnr(i);
    auto object = _memory_regions[i].get();
    if (object->is_address_range()) {
      auto range = static_cast<AddressRange *>(object);
      auto region = range->get_region_for_brand(_memory_regions[i].get_brand());
      mpu.write_rbar(region.rbar);
      mpu.write_rasr(region.rasr);
    } else {
      mpu.write_rasr(0);
      mpu.write_rbar(0);
    }
  }
}

void Context::make_runnable() {
  switch (_state) {
    case State::sending:
      complete_blocked_send(Exception::would_block);
      break;

    case State::receiving:
      complete_blocked_receive(Exception::would_block);
      break;

    case State::stopped:
      runnable.insert(&_ctx_item);
      _state = State::runnable;
      break;

    case State::runnable:
      break;
  }
}


/*******************************************************************************
 * Implementation of Sender and BlockingSender
 */

Priority Context::get_priority() const {
  return _priority;
}

Message Context::get_message() {
  return _save.sys.m;
}

void Context::complete_send() {
  auto d = get_descriptor();

  if (d.get_receive_enabled()) {
    auto source = d.is_call() ? make_reply_key()
                              : key(d.get_source());
    source.get()->deliver_to(this);
  }
}

void Context::complete_send(Exception e, uint32_t param) {
  // We may have distributed a key to our reply gate earlier in the send phase.
  if (get_descriptor().is_call()) {
    // If so, revoke it, since the send has been cancelled.
    object_table.invalidate(_reply_gate_index);
  }
  // Deliver the exception.
  put_message(0, Message::failure(e, param));
  // Avoid looking like we delivered any pre-existing keys.
  nullify_exchanged_keys();
}

void Context::block_in_send(Brand brand, List<BlockingSender> & list) {
  ETL_ASSERT(this == current);

  if (get_descriptor().get_block()) {
    _saved_brand = brand;
    list.insert(&_sender_item);
    _ctx_item.unlink();
    _state = State::sending;

    pend_switch();
  } else {
    // Unprivileged code is unwilling to block for delivery.
    complete_send(Exception::would_block);
  }
}

Brand Context::get_saved_brand() const {
  return _saved_brand;
}

void Context::complete_blocked_send() {
  runnable.insert(&_ctx_item);
  _state = State::runnable;

  complete_send();
  pend_switch();
}

void Context::complete_blocked_send(Exception e, uint32_t param) {
  runnable.insert(&_ctx_item);
  _state = State::runnable;

  complete_send(e, param);
  pend_switch();
}

Key Context::get_message_key(unsigned index) {
  if (index == 0) {
    // Special handling of reply key
    auto d = get_descriptor();
    if (d.is_call()) {
      return make_reply_key();
    } else {
      return Key::null();
    }
  } else {
    // Other keys
    return key(index);
  }
}

Key Context::make_reply_key() const {
  auto maybe_key = object_table[_reply_gate_index].ptr->make_key(0);
  if (maybe_key) return maybe_key.ref();
  return Key::null();
}


/*******************************************************************************
 * Implementation of Object
 */

void Context::deliver_from(Brand brand, Sender * sender) {
  Message m = sender->get_message();
  switch (m.d0.get_selector()) {
    case 0: 
      read_register(brand, sender, m);
      break;

    case 1: 
      write_register(brand, sender, m);
      break;

    case 2: 
      read_key(brand, sender, m);
      break;

    case 3: 
      write_key(brand, sender, m);
      break;

    case 4: 
      read_region(brand, sender, m);
      break;

    case 5: 
      write_region(brand, sender, m);
      break;

    default:
      sender->complete_send(Exception::bad_operation, m.d0.get_selector());
      break;
  }
}

void Context::read_register(Brand,
                            Sender * sender,
                            Message const & arg) {
  ReplySender reply_sender{0};  // TODO priority
  switch (arg.d1) {
    case 13:
      reply_sender.set_message({
          Descriptor::zero(),
          reinterpret_cast<Word>(_stack),
          });
      break;

#define GP_EF(n) \
    case n: { \
      apply_to_mpu(); \
      auto r = uload(&_stack->r ## n); \
      current->apply_to_mpu(); \
      if (r.is_error()) { \
        reply_sender.set_message(Message::failure(Exception::fault)); \
      } else { \
        reply_sender.set_message({ \
            Descriptor::zero(), \
            r.ref(), \
          }); \
      } \
      break; \
    }
    GP_EF(0);
    GP_EF(1);
    GP_EF(2);
    GP_EF(3);
    GP_EF(12);
    GP_EF(14);
    GP_EF(15);
#undef GP_EF

    case 16: {
      apply_to_mpu();
      auto r = uload(&_stack->psr);
      current->apply_to_mpu();
      if (r.is_error()) {
        reply_sender.set_message(Message::failure(Exception::fault));
      } else {
        reply_sender.set_message({
            Descriptor::zero(),
            r.ref(),
          });
      }
      break;
    }

    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
      reply_sender.set_message({Descriptor::zero(), _save.raw[arg.d1 - 4]});
      break;

    default:
      reply_sender.set_message(Message::failure(Exception::index_out_of_range));
      break;
  }

  auto reply = sender->get_message_key(0);
  sender->complete_send();

  reply.deliver_from(&reply_sender);
}

void Context::write_register(Brand,
                             Sender * sender,
                             Message const & arg) {
  auto r = arg.d1;
  auto v = arg.d2;

  ReplySender reply_sender{0};  // TODO priority

  switch (r) {
    case 13:
      _stack = reinterpret_cast<decltype(_stack)>(v);
      break;

#define GP_EF(n) \
    case n: { \
      auto r = ustore(&_stack->r ## n, v); \
      if (r != SysResult::success) { \
        reply_sender.set_message(Message::failure(Exception::fault)); \
      } \
      break; \
    }
    GP_EF(0);
    GP_EF(1);
    GP_EF(2);
    GP_EF(3);
    GP_EF(12);
    GP_EF(14);
    GP_EF(15);
#undef GP

    case 16: {
      auto r = ustore(&_stack->psr, v);
      if (r != SysResult::success) {
        reply_sender.set_message(Message::failure(Exception::fault));
      }
      break;
    }

    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
      _save.raw[r - 4] = v;
      break;

    default:
      reply_sender.set_message(Message::failure(Exception::index_out_of_range));
      break;
  }

  auto reply = sender->get_message_key(0);
  sender->complete_send();

  reply.deliver_from(&reply_sender);
}

void Context::read_key(Brand,
                       Sender * sender,
                       Message const & arg) {
  auto r = arg.d1;
  auto reply = sender->get_message_key(0);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority
  if (r >= config::n_task_keys) {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  } else {
    reply_sender.set_key(1, key(r));
  }
  reply.deliver_from(&reply_sender);
}

void Context::write_key(Brand,
                        Sender * sender,
                        Message const & arg) {
  auto r = arg.d1;

  auto reply = sender->get_message_key(0);
  auto new_key = sender->get_message_key(1);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority
  if (r >= config::n_task_keys) {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  } else {
    key(r) = new_key;
  }
  reply.deliver_from(&reply_sender);
}

void Context::read_region(Brand,
                          Sender * sender,
                          Message const & arg) {
  auto n = arg.d1;
  auto reply = sender->get_message_key(0);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority
  if (n < config::n_task_regions) {
    reply_sender.set_key(1, _memory_regions[n]);
  } else {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  }
  reply.deliver_from(&reply_sender);
}

void Context::write_region(Brand,
                           Sender * sender,
                           Message const & arg) {
  auto n = arg.d1;
  auto reply = sender->get_message_key(0);
  auto object_key = sender->get_message_key(1);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority

  if (n < config::n_task_regions) {
    _memory_regions[n] = object_key;
  } else {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  }

  if (current == this) apply_to_mpu();

  reply.deliver_from(&reply_sender);
}

}  // namespace k
