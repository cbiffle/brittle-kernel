.syntax unified

@
@ Loads
@

@ The uload_impl functions are generated using this macro, parameterized
@ in terms of the load size (empty string, h, or b).
.macro safe_load size
    ldr r2, =_ZN1k25mm_fault_recovery_handlerE
                                          @ Get address of handler cell.
    adr r3, 1f                            @ Get address of our handler.
    str r3, [r2]                          @ Set up our handler.

    ldr\size\()t r3, [r0]                 @ Unprivileged load.

    movs r0, #0                           @ Set up result.
    str r0, [r2]                          @ Reuse to zero handler.
    
    str\size\() r3, [r1]                  @ Stash result.
    bx lr

1:  movs r0, #1                           @ Indicate failure.
    bx lr
.endm

.globl _ZN1k10uload_implEPKmPm
.thumb_func
_ZN1k10uload_implEPKmPm:
    safe_load

.globl _ZN1k10uload_implEPKhPh
.thumb_func
_ZN1k10uload_implEPKhPh:
    safe_load b


@
@ Stores
@

@ The ustore_impl functions are generated using this macro, parameterized
@ in terms of the load size (empty string, h, or b).
.macro safe_store size
    ldr r2, =_ZN1k25mm_fault_recovery_handlerE
                                          @ Get address of handler cell.
    adr r3, 1f                            @ Get address of our handler.
    str r3, [r2]                          @ Set up our handler.

    str\size\()t r1, [r0]                 @ Unprivileged store.

    movs r0, #0                           @ Set up result.
    str r0, [r2]                          @ Reuse to zero handler.
    bx lr

1:  movs r0, #1                           @ Indicate failure.
    bx lr
.endm

.globl _ZN1k11ustore_implEPmm
.thumb_func
_ZN1k11ustore_implEPmm:
    safe_store

.globl _ZN1k11ustore_implEPhh
.thumb_func
_ZN1k11ustore_implEPcc:
    safe_store b


