.syntax unified

.balign 4
.globl _ZN2rt3ipcERK7Messagemm
.thumb_func
_ZN2rt3ipcERK7Messagemm:
    push {r4-r11, lr}                   @ Free up transfer registers.
    ldm r1, {r4-r9}                     @ Load message being sent.
    mov r10, r2                         @ Move send map into place.
    mov r11, r3                         @ Move receive map into place.
    svc #0                              @ Syscall!
    stm r0, {r4-r10}                    @ Store received message.
    pop {r4-r11, pc}                    @ Restore and return.
