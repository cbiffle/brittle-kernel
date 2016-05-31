.syntax unified

.section .srv.ascii, "ax", %progbits
.globl _prog_srv_ascii
_prog_srv_ascii:
.incbin "gen/a/srv/ascii/ascii.bin"
