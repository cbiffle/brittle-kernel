MEMORY {
  /* Alias of the SRAM112 region when it's remapped. */
  sram112_at_zero (rwx) : ORIGIN = 0x00000000, LENGTH =  112K

  rom (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  ccm (rwx)     : ORIGIN = 0x10000000, LENGTH =   64K
  sram112 (rwx) : ORIGIN = 0x20000000, LENGTH =  112K
  sram16  (rwx) : ORIGIN = 0x2001c000, LENGTH =   16K
}

EXTERN(_ZN3etl6armv7mL15exception_tableE)
EXTERN(_ZN4demo4mainEv)

ENTRY(etl_armv7m_reset_handler)

SECTIONS {
  .vector_tables : {
    KEEP(*(.etl_armv7m_exception_table*))
    KEEP(*(.etl_stm32f4xx_interrupt_table))
    /* Include initialized data here, so we initialize it during the copy. */
    /* We use a word copy, so ensure alignment. */
    . = ALIGN(4);
  } >rom

  .demo_text : ALIGN(1024) {
    KEEP(*(.text.startup._ZN4demo4mainEv))
    *(.text._ZN4demo*)
    . = ALIGN(4);
    *(.rodata._ZN4demo*)
  } >rom

  .demo_data : {
    PROVIDE(_demo_data_start = .);
    *(.data.ZN4demo*)
  } >sram112 AT>rom

  .demo_bss (NOLOAD) : ALIGN(4) {
    PROVIDE(_demo_bss_start = .);
    *(.bss._ZN4demo*)
    . = ALIGN(4);
    PROVIDE(_demo_bss_end = .);
    . += 512;
    PROVIDE(_demo_stack = .);
  } >sram112

  .text : {
    . = ADDR(.demo_text) + 1024;
    *(.text*)
    . = ALIGN(4);
    *(.rodata*)
  } >rom

  .init : ALIGN(2) {
    KEEP(*(.init_prologue))
    KEEP(*(.init))
    KEEP(*(.init_epilogue))
  } >rom

  .preinit_array : ALIGN(4) {
    _preinit_array_start = .;
    KEEP(*(.preinit_array))
    _preinit_array_end = .;
  } >rom

  .init_array : ALIGN(4) {
    _init_array_start = .;
    KEEP(*(.init_array))
    _init_array_end = .;
  } >rom

  .data : {
    *(.data*)
  } >ccm AT>rom

  PROVIDE(_data_init_image_start = LOADADDR(.data));
  PROVIDE(_data_start = ADDR(.data));
  PROVIDE(_data_end = ADDR(.data) + SIZEOF(.data));

  .bss (NOLOAD) : {
    . += 512;  /* Leave room for a stack. */
    PROVIDE(etl_armv7m_initial_stack_top = .);
    . += 512;  /* HACK padding */
    . = ALIGN(4);
    PROVIDE(_bss_start = .);
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    PROVIDE(_bss_end = .);
  } >ccm
}
