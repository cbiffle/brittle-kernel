#include "k/scheduler.h"

#include "etl/armv7m/scb.h"

#include "k/context.h"
#include "k/list.h"

using etl::armv7m::scb;
using etl::armv7m::Scb;

namespace k {

Context * current;
List<Context> runnable;

static bool switch_pending;

void pend_switch() {
  switch_pending = true;
}

static void switch_now() {
  auto head = runnable.peek();

  if (config::checks == false) {
    // We still want to check this!
    ETL_ASSERT(head);
  }

  current = head.ref()->owner;
}

void do_deferred_switch() {
  if (!switch_pending) return;

  switch_pending = false;

  switch_now();
}

void do_deferred_switch_from_irq() {
  if (switch_pending) {
    switch_pending = false;

    scb.write_icsr(Scb::icsr_value_t().with_pendsvset(true));
  }
}

void * switch_after_interrupt(void * stack) {
  current->set_stack(static_cast<StackRegisters *>(stack));
  switch_now();
  return current->stack();
}

}  // namespace k
