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
  static_assert(K_CONTEXT_STACK_OFFSET == __builtin_offsetof(Context, _stack),
                "K_CONTEXT_STACK_OFFSET is wrong");
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

void * Context::do_ipc(void * stack, Descriptor d) {
  set_stack(static_cast<StackRegisters *>(stack));

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

void Context::do_copy_key(Descriptor d) {
  key(d.get_target()) = key(d.get_source());
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
      _sender_item.unlink();
      on_blocked_delivery_failed(Exception::would_block);
      break;

    case State::receiving:
      _ctx_item.unlink();
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

using IpcImpl = void (Context::*)(ScopedReplySender &,
                                  Brand const &,
                                  Message const &,
                                  Keys &);

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
    &Context::do_save_kernel_registers,
    &Context::do_restore_kernel_registers,
  };
  if (m.d0.get_selector() < etl::array_count(dispatch)) {
    ScopedReplySender reply_sender{k.keys[0]};
    auto fn = dispatch[m.d0.get_selector()];
    (this->*fn)(reply_sender, brand, m, k);
  } else {
    do_badop(m, k);
  }
}

auto Context::lookup_register(unsigned r) -> Maybe<RegisterLocation> {
  switch (r) {
#define EFR(n, name) \
    case n: \
      return {{ &_stack->name, false }};
    EFR(0, r0)
    EFR(1, r1)
    EFR(2, r2)
    EFR(3, r3)
    EFR(12, r12)
    EFR(14, r14)
    EFR(15, r15)
    EFR(16, psr)
#undef EFR

    case 4 ... 11:
      return {{ &_save.raw[r - 4], true }};

    case 13:
      return {{ reinterpret_cast<uint32_t *>(&_stack), true }};

    case 17:
      return {{ &_save.named.basepri, true }};

    default:
      return nothing;
  }
}

void Context::do_read_register(ScopedReplySender & reply_sender,
                               Brand const &,
                               Message const & arg,
                               Keys &) {
  auto maybe_rloc = lookup_register(arg.d1);

  if (!maybe_rloc) {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
    return;
  }

  auto & rloc = maybe_rloc.ref();
  if (rloc.in_context) {
    reply_sender.get_message().d1 = *rloc.addr;
  } else {
    apply_to_mpu();
    auto maybe_value = uload(rloc.addr);
    current->apply_to_mpu();
    if (maybe_value) {
      reply_sender.get_message().d1 = maybe_value.ref();
    } else {
      reply_sender.get_message() = Message::failure(Exception::fault);
    }
  }
}

void Context::do_write_register(ScopedReplySender & reply_sender,
                                Brand const &,
                                Message const & arg,
                                Keys &) {
  auto maybe_rloc = lookup_register(arg.d1);

  if (!maybe_rloc) {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
    return;
  }

  auto & rloc = maybe_rloc.ref();
  if (rloc.in_context) {
    *rloc.addr = arg.d2;
  } else {
    apply_to_mpu();
    if (!ustore(rloc.addr, arg.d2)) {
      reply_sender.get_message() = Message::failure(Exception::fault);
    }
    current->apply_to_mpu();
  }
}

void Context::do_read_key(ScopedReplySender & reply_sender,
                          Brand const &,
                          Message const & arg,
                          Keys &) {
  auto r = arg.d1;

  if (r >= config::n_task_keys) {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
  } else {
    reply_sender.set_key(1, key(r));
  }
}

void Context::do_write_key(ScopedReplySender & reply_sender,
                           Brand const &,
                           Message const & arg,
                           Keys & k) {
  auto r = arg.d1;

  auto & new_key = k.keys[1];

  if (r >= config::n_task_keys) {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
  } else {
    key(r) = new_key;
  }
}

void Context::do_read_region(ScopedReplySender & reply_sender,
                             Brand const &,
                             Message const & arg,
                             Keys &) {
  auto n = arg.d1;

  if (n < config::n_task_regions) {
    reply_sender.set_key(1, _memory_regions[n]);
  } else {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
  }
}

void Context::do_write_region(ScopedReplySender & reply_sender,
                              Brand const &,
                              Message const & arg,
                              Keys & k) {
  auto n = arg.d1;
  auto & object_key = k.keys[1];

  if (n < config::n_task_regions) {
    _memory_regions[n] = object_key;
  } else {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
  }

  if (current == this) apply_to_mpu();
}

void Context::do_make_runnable(ScopedReplySender &,
                               Brand const &,
                               Message const &,
                               Keys &) {
  make_runnable();
  pend_switch();
}

void Context::do_read_priority(ScopedReplySender & reply_sender,
                               Brand const &,
                               Message const &,
                               Keys &) {
  reply_sender.get_message().d1 = _priority;
}

void Context::do_write_priority(ScopedReplySender & reply_sender,
                                Brand const &,
                                Message const & arg,
                                Keys &) {
  auto priority = arg.d1;

  if (priority < config::n_priorities) {
    _priority = priority;
  
    if (_ctx_item.is_linked()) _ctx_item.reinsert();
    if (_sender_item.is_linked()) _sender_item.reinsert();
  } else {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
  }
}

void Context::do_save_kernel_registers(ScopedReplySender & reply_sender,
                                       Brand const &,
                                       Message const & arg,
                                       Keys &) {
  uint32_t * dest = reinterpret_cast<uint32_t *>(arg.d1);

  for (unsigned i = 0; i < etl::array_count(_save.raw); ++i) {
    if (!ustore(&dest[i], _save.raw[i])) {
      reply_sender.get_message() = Message::failure(Exception::fault);
      return;
    }
  }
}

void Context::do_restore_kernel_registers(ScopedReplySender & reply_sender,
                                          Brand const &,
                                          Message const & arg,
                                          Keys &) {
  uint32_t const * src = reinterpret_cast<uint32_t const *>(arg.d1);

  for (unsigned i = 0; i < etl::array_count(_save.raw); ++i) {
    if (auto mval = uload(&src[i])) {
      _save.raw[i] = mval.ref();
    } else {
      reply_sender.get_message() = Message::failure(Exception::fault);
      return;
    }
  }
}

}  // namespace k
