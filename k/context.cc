#include "k/context.h"

namespace k {

Context contexts[config::n_contexts];
Context * current;

SysResult Context::call(uint32_t brand, Message const * m, Message *) {
  // TODO unprivileged access
  return SysResult::bad_key;
}

void Context::nullify_exchanged_keys() {
  for (unsigned i = 0; i < config::n_message_keys; ++i) {
    _keys[i].nullify();
  }
}

}  // namespace k
