MEMORY {
  rom (rx)      : ORIGIN = 0x08004000, LENGTH = 1016K
  sram112 (rwx) : ORIGIN = 0x20000000, LENGTH =  112K
  sram16  (rwx) : ORIGIN = 0x2001c000, LENGTH =   16K
}

ENTRY(_ZN4demo4mainEv)

SECTIONS {
  .text : {
    KEEP(*(.text.startup._ZN4demo4mainEv))
    *(.text*)
    . = ALIGN(4);
    *(.rodata*)
  } >rom

  .data : {
    PROVIDE(_demo_data_start = .);
    *(.data*)
  } >sram112 AT>rom

  .bss (NOLOAD) : ALIGN(4) {
    PROVIDE(_demo_bss_start = .);
    *(.bss*)
    . = ALIGN(4);
    PROVIDE(_demo_bss_end = .);
  } >sram112
}
