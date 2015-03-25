#ifndef K_CONTEXT_H
#define K_CONTEXT_H

#include <stdint.h>

#include "k/config.h"
#include "k/key.h"
#include "k/object.h"

namespace k {

struct Registers;  // see: k/registers.h

/*
 * A hardware execution context -- the kernel side of what an application
 * might call a task.
 *
 * Contexts model only the unprivileged execution environment of the processor,
 * plus some state needed by the kernel and scheduler.
 *
 * A context key gives authority to inspect and alter this machine state, *not*
 * to communicate with the code it describes; that authority would be conferred
 * by a gate key.
 */
class Context : public Object {
public:
  SysResult call(uint32_t, Message const *, Message *) override;

  // Accessors for use inside the kernel.
  Registers * stack() const { return _stack; }
  void set_stack(Registers * s) { _stack = s; }
  Key & key(unsigned i) { return _keys[i]; }

  void nullify_exchanged_keys(unsigned preserved = 0);

private:
  // Address of the top of the context's current stack.  When the task
  // is stopped, the machine registers are pushed onto this stack.
  Registers * _stack;
  // Keys held by the context.
  Key _keys[config::n_task_keys];

  SysResult read_register(uint32_t, Message const *, Message *);
  SysResult write_register(uint32_t, Message const *, Message *);

  SysResult read_key(uint32_t, Message const *, Message *);
  SysResult write_key(uint32_t, Message const *, Message *);
};

extern Context contexts[config::n_contexts];
extern Context * current;

}  // namespace k

#endif  // K_CONTEXT_H
