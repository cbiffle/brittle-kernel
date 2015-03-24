#ifndef K_CONTEXT_H
#define K_CONTEXT_H

#include <stdint.h>

#include "k/config.h"
#include "k/key.h"

namespace k {

struct Registers;  // see: k/registers.h

struct Context {
  // Address of the top of the context's current stack.  When the task
  // is stopped, the machine registers are pushed onto this stack.
  Registers * stack;
  // Keys held by the context.
  Key keys[config::n_task_keys];
};

extern Context contexts[config::n_contexts];
extern Context * current;

}  // namespace k

#endif  // K_CONTEXT_H
