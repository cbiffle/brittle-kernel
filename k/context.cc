#include "k/context.h"

#include "etl/armv7m/types.h"
#include "etl/error/check.h"

#include "k/registers.h"
#include "k/ipc.h"
#include "k/unprivileged.h"

using etl::armv7m::Word;

namespace k {

Context contexts[config::n_contexts];
Context * current;

void Context::nullify_exchanged_keys(unsigned preserved) {
  for (unsigned i = preserved; i < config::n_message_keys; ++i) {
    _keys[i].nullify();
  }
}

SysResultWith<Message> Context::get_message() {
  auto arg = reinterpret_cast<Message const *>(_stack->ef.r1);
  return uload(arg);
}

SysResult Context::put_message(Message const & m) {
  auto addr = reinterpret_cast<Message *>(_stack->ef.r2);
  return ustore(addr, m);
}

SysResult Context::call(uint32_t brand, Context * caller) {
  Message m = CHECK(caller->get_message());
  switch (m.data[0]) {
    case 0: return read_register(brand, caller, m);
    case 1: return write_register(brand, caller, m);
    case 2: return read_key(brand, caller, m);
    case 3: return write_key(brand, caller, m);

    default:
      return SysResult::bad_message;
  }
}

SysResult Context::read_register(uint32_t,
                                 Context * caller,
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

  CHECK(caller->put_message({value, 0, 0, 0}));
  caller->nullify_exchanged_keys();
  return SysResult::success;
}

SysResult Context::write_register(uint32_t,
                                  Context * caller,
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

  CHECK(caller->put_message({0,0,0,0}));
  caller->nullify_exchanged_keys();
  return SysResult::success;
}

SysResult Context::read_key(uint32_t,
                            Context * caller,
                            Message const & arg) {
  auto r = arg.data[1];
  if (r >= config::n_task_keys) return SysResult::bad_message;

  CHECK(caller->put_message({0, 0, 0, 0}));
  current->key(0) = key(r);
  current->nullify_exchanged_keys(1);
  return SysResult::success;
}

SysResult Context::write_key(uint32_t,
                             Context * caller,
                             Message const & arg) {
  auto r = arg.data[1];
  if (r >= config::n_task_keys) return SysResult::bad_message;

  CHECK(caller->put_message({0, 0, 0, 0}));
  key(r) = current->key(0);
  current->nullify_exchanged_keys();
  return SysResult::success;
}

}  // namespace k
