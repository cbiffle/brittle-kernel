#ifndef K_CONTEXT_H
#define K_CONTEXT_H

#include <stdint.h>

#include "k/config.h"

namespace k {

struct Context {
  // Address of the top of the context's current stack.
  uintptr_t stack;
};

extern Context contexts[config::n_contexts];
extern Context * current;

}  // namespace k

#endif  // K_CONTEXT_H
