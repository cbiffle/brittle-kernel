#include "k/context.h"

#include "etl/array_count.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/types.h"

#include "common/message.h"
#include "common/descriptor.h"

#include "k/address_range.h"
#include "k/context_layout.h"
#include "k/object_table.h"
#include "k/registers.h"
#include "k/reply_sender.h"
#include "k/scheduler.h"
#include "k/unprivileged.h"

using etl::armv7m::Word;

namespace k {

/*******************************************************************************
 * Context-specific stuff
 */

Context::Context()
  : _stack{nullptr},
    _save{},
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
    _keys[i] = Key::null();
  }
}

Descriptor Context::get_descriptor() const {
  return _save.sys.m.d0;
}

Keys & Context::get_message_keys() {
  return *reinterpret_cast<Keys *>(_keys);
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
  sender->on_blocked_delivery_accepted(_save.sys.m,
                                       _save.sys.b,
                                       get_message_keys());
}

void Context::complete_receive(Exception e, uint32_t param) {
  _save.sys.m = Message::failure(e, param);
  _save.sys.b = 0;
  nullify_exchanged_keys();
}

void Context::block_in_receive(List<Context> & list) {
  // TODO should we decide to permit non-blocking recieves... here's the spot.
  _ctx_item.unlink();
  list.insert(&_ctx_item);
  _state = State::receiving;

  pend_switch();
}

void Context::complete_blocked_receive(Brand const & brand, Sender * sender) {
  runnable.insert(&_ctx_item);
  _state = State::runnable;
  _save.sys.b = brand;

  pend_switch();

  sender->on_delivery_accepted(_save.sys.m, get_message_keys());
}

void Context::complete_blocked_receive(Exception e, uint32_t param) {
  runnable.insert(&_ctx_item);
  _state = State::runnable;

  pend_switch();

  complete_receive(e, param);
}

void Context::put_message(Brand brand, Message const & m) {
  _save.sys.m = m.sanitized();
  _save.sys.b = brand;
}

void Context::apply_to_mpu() {
  using etl::armv7m::mpu;

  for (unsigned i = 0; i < config::n_task_regions; ++i) {
    mpu.write_rnr(i);
    auto object = _memory_regions[i].get();
    // TODO: this cast pattern is sub-optimal.  Better to have a cast method.
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
      on_blocked_delivery_failed(Exception::would_block);
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

void Context::on_delivery_accepted(Message & m, Keys & k) {
  auto d = get_descriptor();

  m = _save.sys.m.sanitized();

  k.keys[0] = d.is_call() ? make_reply_key() : Key::null();
  for (unsigned ki = 1; ki < config::n_message_keys; ++ki) {
    k.keys[ki] = key(ki);
  }

  if (d.get_receive_enabled()) {
    auto & source = d.is_call() ? k.keys[0]
                                : key(d.get_source());
    source.get()->deliver_to(this);
  }
}

void Context::on_delivery_failed(Exception e, uint32_t param) {
  // Deliver the exception.
  _save.sys.m = Message::failure(e, param);
  _save.sys.b = 0;
  // Avoid looking like we delivered any pre-existing keys.
  nullify_exchanged_keys();
}

void Context::block_in_send(Brand const & brand, List<BlockingSender> & list) {
  ETL_ASSERT(this == current);

  if (get_descriptor().get_block()) {
    _saved_brand = brand;
    list.insert(&_sender_item);
    _ctx_item.unlink();
    _state = State::sending;

    pend_switch();
  } else {
    // Unprivileged code is unwilling to block for delivery.
    on_delivery_failed(Exception::would_block);
  }
}

void Context::on_blocked_delivery_accepted(Message & m, Brand & b, Keys & k) {
  runnable.insert(&_ctx_item);
  _state = State::runnable;

  b = _saved_brand;
  pend_switch();
  on_delivery_accepted(m, k);
}

void Context::on_blocked_delivery_failed(Exception e, uint32_t param) {
  runnable.insert(&_ctx_item);
  _state = State::runnable;
  pend_switch();

  on_delivery_failed(e, param);
}

Key Context::make_reply_key() const {
  auto maybe_key = object_table[_reply_gate_index].ptr->make_key(0);
  if (maybe_key) return maybe_key.ref();
  return Key::null();
}


/*******************************************************************************
 * Implementation of Context protocol.
 */

using IpcImpl = void (Context::*)(Brand const &, Message const &, Keys &);

void Context::deliver_from(Brand const & brand, Sender * sender) {
  Message m;
  Keys k;
  sender->on_delivery_accepted(m, k);

  static constexpr IpcImpl dispatch[] {
    &Context::do_read_register,
    &Context::do_write_register,
    &Context::do_read_key,
    &Context::do_write_key,
    &Context::do_read_region,
    &Context::do_write_region,
    &Context::do_make_runnable,
    &Context::do_read_priority,
    &Context::do_write_priority,
  };
  if (m.d0.get_selector() < etl::array_count(dispatch)) {
    auto fn = dispatch[m.d0.get_selector()];
    (this->*fn)(brand, m, k);
  } else {
    do_badop(m, k);
  }
}

void Context::do_read_register(Brand const &,
                               Message const & arg,
                               Keys & k) {
  ReplySender reply_sender;
  switch (arg.d1) {
    case 13:
      reply_sender.set_message({
          Descriptor::zero(),
          reinterpret_cast<Word>(_stack),
          });
      break;

#define GP_EF(num, nam) \
    case num: { \
      apply_to_mpu(); \
      auto r = uload(&_stack->nam); \
      current->apply_to_mpu(); \
      if (r) { \
        reply_sender.set_message({ \
            Descriptor::zero(), \
            r.ref(), \
          }); \
      } else { \
        reply_sender.set_message(Message::failure(Exception::fault)); \
      } \
      break; \
    }
    GP_EF(0, r0);
    GP_EF(1, r1);
    GP_EF(2, r2);
    GP_EF(3, r3);
    GP_EF(12, r12);
    GP_EF(14, r14);
    GP_EF(15, r15);
    GP_EF(16, psr);
#undef GP_EF

    case 4 ... 11:
      reply_sender.set_message({Descriptor::zero(), _save.raw[arg.d1 - 4]});
      break;

    case 17:  // BASEPRI
      reply_sender.set_message({Descriptor::zero(), _save.named.basepri});
      break;

    default:
      reply_sender.set_message(Message::failure(Exception::index_out_of_range));
      break;
  }

  k.keys[0].deliver_from(&reply_sender);
}

void Context::do_write_register(Brand const &,
                                Message const & arg,
                                Keys & k) {
  auto r = arg.d1;
  auto v = arg.d2;

  ReplySender reply_sender;

  switch (r) {
    case 13:
      _stack = reinterpret_cast<decltype(_stack)>(v);
      break;

#define GP_EF(num, nam) \
    case num: { \
      if (!ustore(&_stack->nam, v)) { \
        reply_sender.set_message(Message::failure(Exception::fault)); \
      } \
      break; \
    }
    GP_EF(0, r0);
    GP_EF(1, r1);
    GP_EF(2, r2);
    GP_EF(3, r3);
    GP_EF(12, r12);
    GP_EF(14, r14);
    GP_EF(15, r15);
    GP_EF(16, psr);
#undef GP

    case 4 ... 11:
      _save.raw[r - 4] = v;
      break;

    case 17:  // BASEPRI
      _save.named.basepri = v;
      break;

    default:
      reply_sender.set_message(Message::failure(Exception::index_out_of_range));
      break;
  }

  k.keys[0].deliver_from(&reply_sender);
}

void Context::do_read_key(Brand const &,
                          Message const & arg,
                          Keys & k) {
  auto r = arg.d1;

  ReplySender reply_sender;
  if (r >= config::n_task_keys) {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  } else {
    reply_sender.set_key(1, key(r));
  }
  k.keys[0].deliver_from(&reply_sender);
}

void Context::do_write_key(Brand const &,
                           Message const & arg,
                           Keys & k) {
  auto r = arg.d1;

  auto & reply = k.keys[0];
  auto & new_key = k.keys[1];

  ReplySender reply_sender;
  if (r >= config::n_task_keys) {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  } else {
    key(r) = new_key;
  }
  reply.deliver_from(&reply_sender);
}

void Context::do_read_region(Brand const &,
                             Message const & arg,
                             Keys & k) {
  auto n = arg.d1;

  ReplySender reply_sender;
  if (n < config::n_task_regions) {
    reply_sender.set_key(1, _memory_regions[n]);
  } else {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  }
  k.keys[0].deliver_from(&reply_sender);
}

void Context::do_write_region(Brand const &,
                              Message const & arg,
                              Keys & k) {
  auto n = arg.d1;
  auto & reply = k.keys[0];
  auto & object_key = k.keys[1];

  ReplySender reply_sender;

  if (n < config::n_task_regions) {
    _memory_regions[n] = object_key;
  } else {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  }

  if (current == this) apply_to_mpu();

  reply.deliver_from(&reply_sender);
}

void Context::do_make_runnable(Brand const &, Message const & arg, Keys & k) {
  make_runnable();
  pend_switch();

  ReplySender reply_sender;
  k.keys[0].deliver_from(&reply_sender);
}

void Context::do_read_priority(Brand const &, Message const & arg, Keys & k) {
  ReplySender reply_sender;
  reply_sender.set_message({Descriptor::zero(), _priority});
  k.keys[0].deliver_from(&reply_sender);
}

void Context::do_write_priority(Brand const &, Message const & arg, Keys & k) {
  auto priority = arg.d1;

  ReplySender reply_sender;

  if (priority < config::n_priorities) {
    _priority = priority;
  
    if (_ctx_item.is_linked()) _ctx_item.reinsert();
    if (_sender_item.is_linked()) _sender_item.reinsert();
  } else {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  }

  k.keys[0].deliver_from(&reply_sender);
}

}  // namespace k
