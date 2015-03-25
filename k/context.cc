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

void Context::nullify_exchanged_keys() {
  for (unsigned i = 0; i < config::n_message_keys; ++i) {
    _keys[i].nullify();
  }
}

SysResult Context::call(uint32_t brand, Message const * arg, Message * result) {
  switch (CHECK(uload(&arg->data[0]))) {
    case 0: return read_register(brand, arg, result);
    case 1: return write_register(brand, arg, result);
    case 2: return read_key(brand, arg, result);
    case 3: return write_key(brand, arg, result);

    default:
      return SysResult::bad_message;
  }
}

SysResult Context::read_register(uint32_t,
                                 Message const * arg,
                                 Message * result) {
  auto r = CHECK(uload(&arg->data[1]));
  Word value;
  switch (r) {
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

  return ustore(result, {value, 0, 0, 0});
}

SysResult Context::write_register(uint32_t,
                                  Message const * arg,
                                  Message * result) {
  auto r = CHECK(uload(&arg->data[1]));
  auto v = CHECK(uload(&arg->data[2]));
  CHECK(ustore(result, {0, 0, 0, 0}));

  switch (r) {
    case 13:
      _stack = reinterpret_cast<decltype(_stack)>(v);
      return SysResult::success;

#define GP_EF(n) case n: return ustore(&_stack->ef.r ## n, v);
    GP_EF(0);
    GP_EF(1);
    GP_EF(2);
    GP_EF(3);
    GP_EF(12);
    GP_EF(14);
    GP_EF(15);
#undef GP

    case 16: return ustore(&_stack->ef.psr, v);

#define GP_KSAV(n) case n: return ustore(&_stack->r ## n, v);
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
}

SysResult Context::read_key(uint32_t,
                            Message const * arg,
                            Message * result) {
  auto r = CHECK(uload(&arg->data[1]));
  if (r >= config::n_task_keys) return SysResult::bad_message;

  current->key(0) = key(r);
  for (unsigned i = 1; i < config::n_message_keys; ++i) {
    current->key(i).nullify();
  }

  return ustore(result, {0, 0, 0, 0});
}

SysResult Context::write_key(uint32_t,
                            Message const * arg,
                            Message * result) {
  auto r = CHECK(uload(&arg->data[1]));
  if (r >= config::n_task_keys) return SysResult::bad_message;

  key(r) = current->key(0);
  current->nullify_exchanged_keys();

  return ustore(result, {0, 0, 0, 0});
}

}  // namespace k
