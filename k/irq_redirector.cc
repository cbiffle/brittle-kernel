#include "k/irq_redirector.h"

#include "k/interrupt.h"

namespace k {

static RangePtr<Interrupt *> redirection_table;

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

}  // namespace k
