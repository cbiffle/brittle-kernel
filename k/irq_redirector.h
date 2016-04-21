#ifndef K_IRQ_REDIRECTOR_H
#define K_IRQ_REDIRECTOR_H

#include "k/range_ptr.h"

namespace k {

class Interrupt;  // see: k/interrupt.h

void set_irq_redirection_table(RangePtr<Interrupt *>);
void reset_irq_redirection_table_for_test();

RangePtr<Interrupt *> get_irq_redirection_table();

}  // namespace k

#endif  // K_IRQ_REDIRECTOR_H
