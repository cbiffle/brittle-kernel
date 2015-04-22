#include "k/scheduler.h"

#include "etl/armv7m/exception_table.h"
#include "etl/armv7m/instructions.h"

#include "k/context.h"
#include "k/list.h"

namespace k {

Context * current;
List<Context> runnable;

static bool switch_pending;

void pend_switch() {
  switch_pending = true;
}

void do_deferred_switch() {
  if (!switch_pending) return;

  switch_pending = false;

  // TODO: we ought to turn on strict Maybe checking in the kernel.
  auto head = runnable.peek();
  ETL_ASSERT(head);

  current = head.ref()->owner;
}

}  // namespace k
