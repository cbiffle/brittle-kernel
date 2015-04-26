#ifndef K_SCHEDULER_H
#define K_SCHEDULER_H

namespace k {

struct Context;  // see: k/context.h
template <typename> struct List;  // see: k/list.h

extern Context * current;

extern List<Context> runnable;

void pend_switch();
void do_deferred_switch();
void do_deferred_switch_from_irq();

void * switch_after_interrupt(void *);

}  // namespace k

#endif  // K_SCHEDULER_H
