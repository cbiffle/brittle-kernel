#include "k/irq_redirector.h"

#include "k/interrupt.h"
#include "k/panic.h"

namespace k {

static RangePtr<Interrupt *> redirection_table;

void set_irq_redirection_table(RangePtr<Interrupt *> t) {
  PANIC_UNLESS(redirection_table.is_empty(), "IRQ redirect table set twice");
  redirection_table = t;
}

void reset_irq_redirection_table_for_test() {
  redirection_table = {};
}

RangePtr<Interrupt *> get_irq_redirection_table() {
  PANIC_IF(redirection_table.is_empty(), "IRQ redirect table not yet set");
  return redirection_table;
}

}  // namespace k
