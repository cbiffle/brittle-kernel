.syntax unified

@ Primitive unprivileged memory access operations.  These are wrapped by
@ more pleasant C++ versions in unprivileged.cc.
@
@ Theory of operation:
@
@ ARMv7-M provides unprivileged load and store operations (with the "t" suffix),
@ which operate as though they appeared in unprivileged code.  If one of these
@ instructions tries to access out-of-bounds memory, the processor takes a
@ Memory Management Fault.
@
@ The kernel's memory management fault handler (see mm_fault.cc) will normally
@ treat such a fault in kernel code as fatal.  However, if a non-zero address
@ has been loaded into k::mm_fault_recovery_handler, it will instead rewrite
@ the stack frame of the faulting kernel code to jump to the address from that
@ variable.
@
@ So, to perform a guarded unprivileged operation, we store a handler address
@ there and then use a t-suffix instruction.  It's effectively a (rather
@ expensive) jump-on-fault.

@
@ Loads
@

@ The uload_impl functions are generated using this macro, parameterized
@ in terms of the size suffix for a load mnemonic (empty string, h, or b).
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

@ Words
.globl _ZN1k10uload_implEPKmPm
.thumb_func
_ZN1k10uload_implEPKmPm:
    safe_load

@ Bytes
.globl _ZN1k10uload_implEPKhPh
.thumb_func
_ZN1k10uload_implEPKhPh:
    safe_load b


@
@ Stores
@

@ The ustore_impl functions are generated using this macro, parameterized
@ in terms of the size suffix for a store mnemonic (empty string, h, or b).
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

@ Words
.globl _ZN1k11ustore_implEPmm
.thumb_func
_ZN1k11ustore_implEPmm:
    safe_store

@ Bytes
.globl _ZN1k11ustore_implEPhh
.thumb_func
_ZN1k11ustore_implEPcc:
    safe_store b
