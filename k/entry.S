#include "k/context_layout.h"

.syntax unified

@ Shorter aliases for long defines.
.equ CTX2BODY, K_CONTEXT_BODY_OFFSET
.equ BODY2ST, K_CONTEXT_BODY_STACK_OFFSET

@ Entry point for system calls (SVC instruction).  This routine dispatches
@ between three different options:
@ - Context::do_ipc (assumed to be the common case)
@ - Context::do_copy_key (fast path)
@ - initial scheduler start (happens once)
.balign 4
.globl etl_armv7m_sv_call_handler
.thumb_func
etl_armv7m_sv_call_handler:
    @ Detect the special case of starting the scheduler, but try to do so
    @ as cheaply as possible -- since it will add overhead to *every* syscall.
    @ The case can be distinguished by the call originating from the Main
    @ Stack, rather than the Process Stack, as indicated by bit 2 of LR being
    @ clear.  By branching out of line on this case, we keep the overhead for
    @ later syscalls to 2 cycles.
    tst lr, #(1 << 2)
    beq 1f

    ldr r3, =_ZN1k7currentE   @ Get '&k::current'
    ldr r0, [r3]              @ Load 'k::current'

    @ Distinguish between IPC and cheap syscalls.  Treat IPC as the fast path.
    lsrs r1, r4, #28          @ Sysnum into r1
    bne 3f                    @ If it's non-zero, trap out.

    @ Call through to 'current->do_ipc(stack, descriptor)'
    mrs r1, PSP               @ Get the unprivileged stack pointer.
    
    mrs r12, BASEPRI          @ Back up BASEPRI setting for saving.
    ldr r2, [r0, #CTX2BODY]   @ Load 'k::current->_body'
    stm r2, {r4-r12}          @ Save callee-save registers.

    mov r2, r4                @ Copy descriptor to pass to context.
    mov r4, lr                @ Back up EXC_RETURN value.
    mov r5, r3                @ Back up '&k::current' for reuse below.

    bl _ZN1k7Context6do_ipcEPv10Descriptor

    mov lr, r4                @ Restore EXC_RETURN so we can trash r4.

2:  msr PSP, r0               @ Set new unprivileged stack pointer.
    ldr r0, [r5]              @ Load 'k::current'.
    ldr r0, [r0, #CTX2BODY]   @ Load 'k::current->_body'.
    ldm r0, {r4-r12}          @ Restore registers from context.
    msr BASEPRI, r12          @ Restore BASEPRI for new context.
    bx lr                     @ Return from exception.

1:  @ Out-of-line path used to start scheduler, activating first context.
    movs r0, #1
    msr CONTROL, r0           @ Shed privileges from Thread mode.

    orr lr, lr, #(1 << 2)     @ Alter EXC_RETURN to use process stack.

    ldr r5, =_ZN1k7currentE   @ Get '&k::current'.

    ldr r0, [r5]              @ Load 'k::current'.
    ldr r0, [r0, #CTX2BODY]   @ Load 'k::current->_body'.
    ldr r0, [r0, #BODY2ST]    @ Load 'k::current->_body.stack'.

    @ The registers are now in the same state that they'd be on return from
    @ Context::do_syscall, above.  Thus, we can reuse the tail of the normal
    @ SVC exit sequence.
    b 2b

3:  @ Cheap syscall path for Copy Key, without a context save.
    @ Note that the sysnum is in r1, if we want to add more syscalls later!
    mov r1, r4                @ Copy descriptor to act as argument.
    b _ZN1k7Context11do_copy_keyE10Descriptor
    

.globl etl_armv7m_pend_sv_handler
.thumb_func
etl_armv7m_pend_sv_handler:
    cpsid i                   @ Block other interrupts using PRIMASK.

    ldr r3, =_ZN1k7currentE   @ Get '&k::current'
    ldr r1, [r3]              @ Load 'k::current'

    @ Call through to 'k::switch_after_interrupt(stack)'
    mrs r0, PSP               @ Get the unprivileged stack pointer.
    
    mrs r12, BASEPRI          @ Back up BASEPRI setting for saving.
    ldr r2, [r1, #CTX2BODY]   @ Load 'k::current->_body'
    stm r2, {r4-r12}          @ Save callee-save registers.

    mov r4, lr                @ Back up EXC_RETURN value.
    mov r5, r3                @ Back up '&k::current' for reuse below.

    bl _ZN1k22switch_after_interruptEPv

    mov lr, r4                @ Restore EXC_RETURN so we can trash r4.

    msr PSP, r0               @ Set new unprivileged stack pointer.
    ldr r0, [r5]              @ Load 'k::current'.
    ldr r0, [r0, #CTX2BODY]   @ Load 'k::current->_body'.
    ldm r0, {r4-r12}          @ Restore registers from context.
    msr BASEPRI, r12          @ Restore BASEPRI for new context.

    cpsie i                   @ Re-enable other interrupts.

    bx lr                     @ Return from exception.

.globl etl_armv7m_mem_manage_fault_handler
.thumb_func
etl_armv7m_mem_manage_fault_handler:
    tst lr, #(1 << 2)         @ Test bit 2 (stack select).
    beq 1f                    @ If clear, we're in the kernel; jump ahead.

    @ Fault was in task code.
    b .

1:  mov r0, sp                @ Get kernel stack pointer.
    b _ZN1k10mm_fault_kEPv    @ Call the C version.
