#include "k/context.h"

namespace k {

Context contexts[config::n_contexts];
Context * current;

SysResult Context::call(uint32_t brand, Message const * m, Message *) {
  // TODO unprivileged access
  return SysResult::bad_key;
}

}  // namespace k
