#include "k/irq_redirector.h"

#include "etl/prediction.h"
#include "etl/armv7m/registers.h"

#include "k/interrupt.h"

namespace k {

static RangePtr<Interrupt *> redirection_table;

void set_irq_redirection_table(RangePtr<Interrupt *> t) {
  ETL_ASSERT(redirection_table.is_empty());
  redirection_table = t;
}

RangePtr<Interrupt *> get_irq_redirection_table() {
  return redirection_table;
}

void irq_redirector() {
  // Subtract 16 from the hardware vector number to get an external interrupt
  // number.  Because we're working in unsigned types, this makes any vector
  // *under* 16 become a very large number, and thus a range violation.
  auto vector_number = (etl::armv7m::get_ipsr() & 0x1FF) - 16;

  auto sender = redirection_table[vector_number];
  ETL_ASSERT(sender);
  sender->trigger();
}

}  // namespace k
