#include "k/context_layout.h"

.syntax unified

@ Shorter aliases for long defines.
.equ KCSO, K_CONTEXT_SAVE_OFFSET
.equ KCST, K_CONTEXT_STACK_OFFSET

.macro kernel_entry c_handler
    mrs r0, PSP               @ Get application stack pointer.
    ldr r1, =_ZN1k7currentE   @ Get address of current context pointer.
    ldr r2, [r1]              @ Get current context.
    add r2, r2, #KCSO         @ Get address of register save area.
    mrs r12, BASEPRI          @ Back up current BASEPRI setting.
    stm r2, {r4-r12}          @ Save callee-save registers into context.

    mov r4, lr                @ Back up exc_return.
    mov r5, r1                @ Back up address of current context pointer.
    bl \c_handler             @ Call the C version.
    mov lr, r4                @ Restore exc_return.

    msr PSP, r0               @ Apply new application stack pointer.
    ldr r1, [r5]              @ Get current (new?) context.
    add r1, r1, #KCSO         @ Get address of register save area.
    ldm r1, {r4-r12}          @ Restore callee-save registers.
    msr BASEPRI, r12          @ Restore BASEPRI.
.endm

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
    lsrs r1, r4, #28          @ Sysnum into r0
    bne 3f                    @ If it's non-zero, trap out.

    @ Call through to 'current->do_ipc(stack, descriptor)'
    mrs r1, PSP               @ Get the unprivileged stack pointer.
    
    mrs r12, BASEPRI          @ Back up BASEPRI setting for saving.
    add r2, r0, #KCSO         @ Compute address of register save area.
    stm r2, {r4-r12}          @ Save callee-save registers.

    mov r2, r4                @ Copy descriptor to pass to context.
    mov r4, lr                @ Back up EXC_RETURN value.
    mov r5, r3                @ Back up '&k::current' for reuse below.

    bl _ZN1k7Context6do_ipcEPv10Descriptor

    mov lr, r4                @ Restore EXC_RETURN so we can trash r4.

2:  msr PSP, r0               @ Set new unprivileged stack pointer.
    ldr r0, [r5]              @ Get the address of current context.
    adds r0, #KCSO            @ Compute address of register save area.
    ldm r0, {r4-r12}          @ Restore registers from context.
    msr BASEPRI, r12          @ Restore BASEPRI for new context.
    bx lr                     @ Return from exception.

1:  @ Out-of-line path used to start scheduler, activating first context.
    movs r0, #1
    msr CONTROL, r0           @ Shed privileges from Thread mode.

    orr lr, lr, #(1 << 2)     @ Alter EXC_RETURN to use process stack.

    ldr r5, =_ZN1k7currentE   @ Get address of current context pointer.

    ldr r0, [r5]              @ Get address of current context.
    ldr r0, [r0, #KCST]       @ Get context stack pointer.

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
    movs r0, #1
    msr PRIMASK, r0
    kernel_entry _ZN1k22switch_after_interruptEPv
    movs r0, #0
    msr PRIMASK, r0
    bx lr

.globl etl_armv7m_mem_manage_fault_handler
.thumb_func
etl_armv7m_mem_manage_fault_handler:
    tst lr, #(1 << 2)         @ Test bit 2 (stack select).
    beq 1f                    @ If clear, we're in the kernel; jump ahead.

    @ Fault was in task code.
    kernel_entry _ZN1k8mm_faultEPv
    bx lr

1:  mov r0, sp                @ Get kernel stack pointer.
    b _ZN1k10mm_fault_kEPv    @ Call the C version.
