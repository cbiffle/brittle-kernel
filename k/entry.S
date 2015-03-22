.syntax unified

.globl etl_armv7m_sv_call_handler
.thumb_func
etl_armv7m_sv_call_handler:
    mov r4, lr                @ Back up exc_return.
    bl _ZN1k12svc_dispatchEv  @ Call the C version.
    bx r4                     @ Finish.
