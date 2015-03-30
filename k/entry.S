#include "k/context_layout.h"

.syntax unified

.equ KCSO, K_CONTEXT_SAVE_OFFSET  @ Shorter alias for long define.

.macro kernel_entry c_handler
    mrs r0, PSP               @ Get application stack pointer.
    ldr r1, =_ZN1k7currentE   @ Get address of current context pointer.
    ldr r2, [r1]              @ Get current context.
    add r2, r2, #KCSO         @ Get address of register save area.
    stm r2, {r4-r11}          @ Save callee-save registers into context.

    mov r4, lr                @ Back up exc_return.
    mov r5, r1                @ Back up address of current context pointer.
    bl \c_handler             @ Call the C version.
    mov lr, r4                @ Restore exc_return.

    msr PSP, r0               @ Apply new application stack pointer.
    ldr r1, [r5]              @ Get current (new?) context.
    add r1, r1, #KCSO         @ Get address of register save area.
    ldm r1, {r4-r11}          @ Restore callee-save registers.
    bx lr                     @ Return.
.endm

.globl etl_armv7m_sv_call_handler
.thumb_func
etl_armv7m_sv_call_handler:
    kernel_entry _ZN1k12svc_dispatchEPv

.globl etl_armv7m_mem_manage_fault_handler
.thumb_func
etl_armv7m_mem_manage_fault_handler:
    mov r0, lr                @ Put exc_return somewhere we can see it.

    lsls r1, r0, #(31 - 2)    @ Get bit 2 (stack select) into N flag
    bpl 1f                    @ If set, we're in the kernel; jump ahead.

    @ Fault was in task code.
    kernel_entry _ZN1k8mm_faultEPv

1:  mov r0, sp                @ Get kernel stack pointer.

    push {lr}                 @ Back up exc_return.
    bl _ZN1k10mm_fault_kEPv   @ Call the C version.
    pop {pc}                  @ Restore exc_return, return.
