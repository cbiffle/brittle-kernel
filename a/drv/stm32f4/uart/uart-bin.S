.syntax unified

.section .drv.stm32f4.uart, "ax", %progbits
.globl _prog_drv_stm32f4_uart
_prog_drv_stm32f4_uart:
.incbin "gen/a/drv/stm32f4/uart/uart.bin"
