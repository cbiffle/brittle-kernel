#include "k/context.h"

#include "etl/armv7m/types.h"
#include "etl/error/check.h"
#include "etl/error/ignore.h"

#include "k/registers.h"
#include "k/reply_sender.h"
#include "k/ipc.h"
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
  for (unsigned i = preserved; i < config::n_message_keys; ++i) {
    _keys[i].nullify();
  }
}

SysResult Context::do_send() {
  // r0 and r1, as part of the exception frame, can be accessed
  // without protection -- we haven't yet allowed for a race that
  // could cause them to become inaccessible.
  auto target_index = _stack->ef.r0;
  auto arg = reinterpret_cast<Message const *>(_stack->ef.r1);

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

  auto r = key(target_index).deliver_from(this);
  if (r != SysResult::success) complete_send(r);
  return SysResult::success;
}


/*******************************************************************************
 * Implementation of Sender
 */

uint32_t Context::get_priority() const {
  return _priority;
}

SysResultWith<Message> Context::get_message() {
  auto arg = reinterpret_cast<Message const *>(_stack->ef.r1);
  return uload(arg);
}

void Context::complete_send(SysResult result) {
  // If the task has set itself up in such a way that the result
  // cannot be reported without faulting, we don't currently do
  // anything special to repair this.  (TODO: message to supervisor)
  IGNORE(ustore(&_stack->ef.r0, unsigned(result)));
}

SysResult Context::block_in_send(uint32_t brand, List<Sender> & list) {
  bool const blocking = true;  // TODO: nonblocking sends

  if (!blocking) return SysResult::would_block;

  _saved_brand = brand;
  list.insert(&_sender_item);
  _ctx_item.unlink();
  return SysResult::success;
}

void Context::complete_blocked_send() {
  // TODO: report faults in the line below to the supervisor.
  IGNORE(ustore(&_stack->ef.r0, uint32_t(SysResult::success)));
  runnable.insert(&_ctx_item);
}

Key Context::get_message_key(unsigned index) {
  return key(index);
}


/*******************************************************************************
 * Implementation of Object
 */

SysResult Context::deliver_from(uint32_t brand, Sender * sender) {
  Message m = CHECK(sender->get_message());
  switch (m.data[0]) {
    case 0: return read_register(brand, sender, m);
    case 1: return write_register(brand, sender, m);
    case 2: return read_key(brand, sender, m);
    case 3: return write_key(brand, sender, m);

    default:
      return SysResult::bad_message;
  }
}

SysResult Context::read_register(uint32_t,
                                 Sender * sender,
                                 Message const & arg) {
  Word value;
  switch (arg.data[1]) {
    case 13:
      value = reinterpret_cast<Word>(_stack);
      break;

#define GP_EF(n) case n: value = CHECK(uload(&_stack->ef.r ## n)); break
    GP_EF(0);
    GP_EF(1);
    GP_EF(2);
    GP_EF(3);
    GP_EF(12);
    GP_EF(14);
    GP_EF(15);
#undef GP_EF

    case 16: value = CHECK(uload(&_stack->ef.psr)); break;

#define GP_KSAV(n) case n: value = CHECK(uload(&_stack->r ## n)); break
    GP_KSAV(4);
    GP_KSAV(5);
    GP_KSAV(6);
    GP_KSAV(7);
    GP_KSAV(8);
    GP_KSAV(9);
    GP_KSAV(10);
    GP_KSAV(11);
#undef GP_KSAV

    default:
      return SysResult::bad_message;
  }

  auto reply = sender->get_message_key(0);
  sender->complete_send();

  ReplySender reply_sender{0, {value}};  // TODO priority
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult Context::write_register(uint32_t,
                                  Sender * sender,
                                  Message const & arg) {
  auto r = arg.data[1];
  auto v = arg.data[2];

  switch (r) {
    case 13:
      _stack = reinterpret_cast<decltype(_stack)>(v);
      break;

#define GP_EF(n) case n: CHECK(ustore(&_stack->ef.r ## n, v)); break;
    GP_EF(0);
    GP_EF(1);
    GP_EF(2);
    GP_EF(3);
    GP_EF(12);
    GP_EF(14);
    GP_EF(15);
#undef GP

    case 16: CHECK(ustore(&_stack->ef.psr, v)); break;

#define GP_KSAV(n) case n: CHECK(ustore(&_stack->r ## n, v));
    GP_KSAV(4);
    GP_KSAV(5);
    GP_KSAV(6);
    GP_KSAV(7);
    GP_KSAV(8);
    GP_KSAV(9);
    GP_KSAV(10);
    GP_KSAV(11);
#undef GP_KSAV

    default:
      return SysResult::bad_message;
  }

  auto reply = sender->get_message_key(0);
  sender->complete_send();

  ReplySender reply_sender{0};  // TODO priority
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult Context::read_key(uint32_t,
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

SysResult Context::write_key(uint32_t,
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

}  // namespace k
