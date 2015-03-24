.syntax unified

.globl etl_armv7m_sv_call_handler
.thumb_func
etl_armv7m_sv_call_handler:
    mrs r0, PSP               @ Get application stack pointer.
    stmfd r0!, {r4-r11}       @ Back up callee-save registers.

    mov r4, lr                @ Back up exc_return.
    bl _ZN1k12svc_dispatchEPv @ Call the C version.
    mov lr, r4                @ Restore.

    ldmfd r0!, {r4-r11}       @ Restore callee-save registers.
    msr PSP, r0               @ Restore application stack pointer.
    bx lr                     @ Return.
