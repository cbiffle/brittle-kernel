#ifndef K_IRQ_REDIRECTOR_H
#define K_IRQ_REDIRECTOR_H

#include "k/range_ptr.h"

namespace k {

class Interrupt;  // see: k/interrupt.h
class SysTick;  // see: k/sys_tick.h

void set_irq_redirection_table(RangePtr<Interrupt *>);

RangePtr<Interrupt *> get_irq_redirection_table();

void irq_redirector();


void set_sys_tick_redirector(SysTick *);
void sys_tick_redirector();

}  // namespace k

#endif  // K_IRQ_REDIRECTOR_H
