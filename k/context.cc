#include "k/context.h"

#include "etl/armv7m/mpu.h"
#include "etl/armv7m/types.h"
#include "etl/error/check.h"
#include "etl/error/ignore.h"

#include "k/context_layout.h"
#include "k/ipc.h"
#include "k/memory_map.h"
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


/*******************************************************************************
 * Context-specific stuff
 */

Context::Context()
  : _stack{nullptr},
    _keys{},
    _ctx_item{this},
    _sender_item{this},
    _saved_brand{0} {}

void Context::nullify_exchanged_keys(unsigned preserved) {
  // Had to do this somewhere, this is as good a place as any.
  static_assert(K_CONTEXT_SAVE_OFFSET == __builtin_offsetof(Context, _registers),
      "K_CONTEXT_SAVE_OFFSET is wrong");
  static_assert(K_CONTEXT_STACK_OFFSET == __builtin_offsetof(Context, _stack),
      "K_CONTEXT_STACK_OFFSET is wrong");
  for (unsigned i = preserved; i < config::n_message_keys; ++i) {
    _keys[i].nullify();
  }
}

SysResult Context::do_send(bool call) {
  // r0 and r1, as part of the exception frame, can be accessed
  // without protection -- we haven't yet allowed for a race that
  // could cause them to become inaccessible.
  auto target_index = _stack->r0;
  auto arg = reinterpret_cast<Message const *>(_stack->r1);

  if (target_index >= config::n_task_keys) {
    return SysResult::bad_key_index;
  }

  // Alright, we're handing off control to some object.  From
  // this point forward we must be more careful with our
  // accesses.  In particular, it is no longer our job (as the
  // dispatcher) to report the result through to the caller;
  // for all we know, the caller is deleted before this returns!
  // So, we must return success to the dispatcher, suppressing
  // its reporting behavior.
  // However, we cannot simply let errors go unreported, nor can
  // we expect every possible implementation of deliver_from to
  // remember to call complete_send.  So we provide it here.

  if (call) {
    key(0).fill(_reply_gate_index, 0);
    _calling = true;
  } else {
    _calling = false;
  }
  auto r = key(target_index).deliver_from(this);
  if (r != SysResult::success) complete_send(r);
  return SysResult::success;
}

SysResult Context::block_in_receive(Brand brand, List<Context> & list) {
  bool const blocking = true;  // TODO: nonblocking receives

  if (!blocking) return SysResult::would_block;

  _saved_brand = brand;
  _ctx_item.unlink();
  list.insert(&_ctx_item);
  return SysResult::success;
}

SysResult Context::complete_blocked_receive(Brand sender_brand,
                                            Sender * sender) {
  // TODO: faults the *sender's* supervisor
  auto m = CHECK(sender->get_message());

  // TODO: report faults in the line below to *our* supervisor.
  CHECK(put_message(_saved_brand, sender_brand, m));

  for (unsigned i = 0; i < config::n_message_keys; ++i) {
    key(i) = sender->get_message_key(i);
  }

  _ctx_item.unlink();  // from the receiver list
  runnable.insert(&_ctx_item);
}

SysResult Context::put_message(Brand gate_brand,
                               Brand sender_brand,
                               Message const & m) {
  auto addr = reinterpret_cast<ReceivedMessage *>(_stack->r2);
  CHECK(ustore(&addr->gate_brand, gate_brand));
  CHECK(ustore(&addr->sender_brand, sender_brand));
  return ustore(&addr->m, m);
}

void Context::apply_to_mpu() {
  using etl::armv7m::mpu;

  for (unsigned i = 0; i < config::n_task_regions; ++i) {
    mpu.write_rnr(i);
    mpu.write_rbar(_regions[i].rbar);
    mpu.write_rasr(_regions[i].rasr);
  }
}


/*******************************************************************************
 * Implementation of Sender
 */

Priority Context::get_priority() const {
  return _priority;
}

SysResultWith<Message> Context::get_message() {
  auto arg = reinterpret_cast<Message const *>(_stack->r1);
  return uload(arg);
}

void Context::complete_send(SysResult result) {
  // If the task has set itself up in such a way that the result
  // cannot be reported without faulting, we don't currently do
  // anything special to repair this.  (TODO: message to supervisor)
  IGNORE(ustore(&_stack->r0, uintptr_t(result)));

  if (result == SysResult::success && _calling) {
    auto rk = key(0);
    nullify_exchanged_keys();
    auto r = rk.deliver_to(this);
    ETL_ASSERT(r == SysResult::success);
  }
}

SysResult Context::block_in_send(Brand brand, List<Sender> & list) {
  bool const blocking = true;  // TODO: nonblocking sends

  if (!blocking) return SysResult::would_block;

  _saved_brand = brand;
  list.insert(&_sender_item);
  _ctx_item.unlink();
  return SysResult::success;
}

void Context::complete_blocked_send() {
  // TODO: report faults in the line below to the supervisor.
  IGNORE(ustore(&_stack->r0, uintptr_t(SysResult::success)));
  runnable.insert(&_ctx_item);
}

Key Context::get_message_key(unsigned index) {
  return key(index);
}


/*******************************************************************************
 * Implementation of Object
 */

SysResult Context::deliver_from(Brand brand, Sender * sender) {
  Message m = CHECK(sender->get_message());
  switch (m.data[0]) {
    case 0: return read_register(brand, sender, m);
    case 1: return write_register(brand, sender, m);
    case 2: return read_key(brand, sender, m);
    case 3: return write_key(brand, sender, m);
    case 4: return read_region(brand, sender, m);
    case 5: return write_region(brand, sender, m);

    default:
      return SysResult::bad_message;
  }
}

SysResult Context::read_register(Brand,
                                 Sender * sender,
                                 Message const & arg) {
  Word value;
  switch (arg.data[1]) {
    case 13:
      value = reinterpret_cast<Word>(_stack);
      break;

#define GP_EF(n) case n: value = CHECK(uload(&_stack->r ## n)); break
    GP_EF(0);
    GP_EF(1);
    GP_EF(2);
    GP_EF(3);
    GP_EF(12);
    GP_EF(14);
    GP_EF(15);
#undef GP_EF

    case 16: value = CHECK(uload(&_stack->psr)); break;

    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
      value = _registers[arg.data[1] - 4];
      break;

    default:
      return SysResult::bad_message;
  }

  auto reply = sender->get_message_key(0);
  sender->complete_send();

  ReplySender reply_sender{0, {value}};  // TODO priority
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult Context::write_register(Brand,
                                  Sender * sender,
                                  Message const & arg) {
  auto r = arg.data[1];
  auto v = arg.data[2];

  switch (r) {
    case 13:
      _stack = reinterpret_cast<decltype(_stack)>(v);
      break;

#define GP_EF(n) case n: CHECK(ustore(&_stack->r ## n, v)); break;
    GP_EF(0);
    GP_EF(1);
    GP_EF(2);
    GP_EF(3);
    GP_EF(12);
    GP_EF(14);
    GP_EF(15);
#undef GP

    case 16: CHECK(ustore(&_stack->psr, v)); break;

    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
      _registers[r - 4] = v;
      break;

    default:
      return SysResult::bad_message;
  }

  auto reply = sender->get_message_key(0);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult Context::read_key(Brand,
                            Sender * sender,
                            Message const & arg) {
  auto r = arg.data[1];
  if (r >= config::n_task_keys) return SysResult::bad_message;

  auto reply = sender->get_message_key(0);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority
  reply_sender.set_key(0, key(r));
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult Context::write_key(Brand,
                             Sender * sender,
                             Message const & arg) {
  auto r = arg.data[1];
  if (r >= config::n_task_keys) return SysResult::bad_message;

  auto reply = sender->get_message_key(0);
  auto new_key = sender->get_message_key(1);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult Context::read_region(Brand,
                               Sender * sender,
                               Message const & arg) {
  auto n = arg.data[1];
  auto reply = sender->get_message_key(0);
  sender->complete_send();

  Message response;
  if (n >= config::n_task_regions) {
    response = {0};
  } else {
    response = {1, uint32_t(_regions[n].rbar), uint32_t(_regions[n].rasr)};
  }
  ReplySender reply_sender{
    0,
    response,
  };
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

static bool region_valid(etl::armv7m::Mpu::rbar_value_t rbar,
                         etl::armv7m::Mpu::rasr_value_t rasr) {
  using etl::armv7m::Mpu;

  // Ironically, the VALID bit must be clear for the region to be valid.
  if (rbar.get_valid()) return false;

  switch (rasr.get_ap()) {
    // Disallow any setting that would prevent privileged access.
    case Mpu::AccessPermissions::p_none_u_none:
    case Mpu::AccessPermissions::p_read_u_none:
    case Mpu::AccessPermissions::p_read_u_read:
      return false;

    default:
      break;
  }

  // Disallow reserved values of SIZE.
  if (rasr.get_size() < 4) return false;

  auto begin = reinterpret_cast<uint8_t *>(rbar.get_addr_27() << 5);
  auto end = begin + (1 << (rasr.get_size() + 1));
  return is_unprivileged_access_ok(begin, end);
}

SysResult Context::write_region(Brand,
                                Sender * sender,
                                Message const & arg) {
  auto n = arg.data[1];
  auto rbar_raw = arg.data[2];
  auto rasr_raw = arg.data[3];
  auto reply = sender->get_message_key(0);
  sender->complete_send();

  Message response;
  auto rbar = etl::armv7m::Mpu::rbar_value_t(rbar_raw);
  auto rasr = etl::armv7m::Mpu::rasr_value_t(rasr_raw);

  if (n >= config::n_task_regions) {
    response = {0};
  } else if (!region_valid(rbar, rasr)) {
    response = {0};
  } else {
    response = {1};
    _regions[n] = { {rbar}, {rasr} };
  }

  ReplySender reply_sender{0, response};  // TODO priority
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

}  // namespace k
