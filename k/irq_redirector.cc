#include "k/irq_redirector.h"

#include "k/interrupt.h"
#include "k/sys_tick.h"

namespace k {

static RangePtr<Interrupt *> redirection_table;
static SysTick * sys_tick_redirect;

void set_irq_redirection_table(RangePtr<Interrupt *> t) {
  ETL_ASSERT(redirection_table.is_empty());
  redirection_table = t;
}

void reset_irq_redirection_table_for_test() {
  redirection_table = {};
}

RangePtr<Interrupt *> get_irq_redirection_table() {
  return redirection_table;
}

void set_sys_tick_redirector(SysTick * irq) {
  ETL_ASSERT(sys_tick_redirect == nullptr);
  sys_tick_redirect = irq;
}

SysTick * get_sys_tick_redirector() {
  ETL_ASSERT(sys_tick_redirect);
  return sys_tick_redirect;
}

}  // namespace k
