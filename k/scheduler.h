#ifndef K_SCHEDULER_H
#define K_SCHEDULER_H

/*
 * Implementation of the Context scheduler.  The scheduler's job is to decide
 * which Context (among the runnable set) should get the CPU next, and to
 * effect hardware context switches between Context objects.
 *
 *
 * Context Switches in Syscalls
 * ----------------------------
 *
 * The unprivileged state of a Context is always saved on synchronous kernel
 * entry (syscall), and always restored on return to unprivileged code.  The
 * save location used is relative to the 'current' Context pointer.  So the
 * act of "switching context" in the kernel is really just a matter of
 * updating 'current'.
 *
 * But changing 'current' at random times might surprise kernel code, and we
 * don't want to pay the cost of marking it volatile.  So we only update
 * 'current' on the way out of the syscall, back to user code.
 *
 * To avoid generating some churn and knocking Contexts out of their timeslices
 * unnecessarily, we only do it if necessary.
 *
 * Any kernel code discovering that a new Context has become runnable, or the
 * current one has blocked, must call 'pend_switch'.  This sets a flag.
 *
 * Later, on the way out of the kernel, 'do_deferred_switch' observes this
 * flag, grovels through the 'runnable' list, and elects a new 'current'.
 *
 *
 * Context Switches in ISRs
 * ------------------------
 *
 * ISRs, including the SysTick Timer, don't follow the normal kernel entry
 * sequence.  To reduce latency, the unprivileged Context's state is not saved
 * and restored in the normal way.  As a result, we can't switch Contexts
 * within an ISR -- directly.
 *
 * Instead, on the way out of an ISR, the kernel calls
 * 'do_deferred_switch_from_isr'.  This pends a PendSV exception.  The PendSV
 * handler *does* fully save unprivileged Context state, and can then perform
 * a normal context switch.
 *
 * This approach ensures that context switches are handled correctly even in
 * the case of interrupts preempting one another.
 */

namespace k {

struct Context;  // see: k/context.h
template <typename> struct List;  // see: k/list.h

/*
 * Scheduler state
 */

// Points to the Context currently mapped to the CPU.
extern Context * current;

// The master list of currently runnable Contexts.  The head of this list is
// the same as 'current'.
extern List<Context> runnable;


/*
 * Deferred switch support.
 */

// Request a deferred context switch (sets a flag).
void pend_switch();

// If 'pend_switch' has been called, updates 'current' with the highest
// priority runnable Context.
void do_deferred_switch();

// If 'pend_switch' has been called, pends a PendSV exception.
void do_deferred_switch_from_irq();

// Implementation of the PendSV handler: saves 'current_stack' as the stack
// pointer in 'current', performs a deferred switch, and returns the new
// stack pointer.
void * switch_after_interrupt(void * current_stack);

}  // namespace k

#endif  // K_SCHEDULER_H
