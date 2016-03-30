MEMORY {
  /* Alias of the SRAM112 region when it's remapped. */
  sram112_at_zero (rwx) : ORIGIN = 0x00000000, LENGTH =  112K

  rom (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  ccm (rwx)     : ORIGIN = 0x10000000, LENGTH =   64K
  sram112 (rwx) : ORIGIN = 0x20000000, LENGTH =  112K
  sram16  (rwx) : ORIGIN = 0x2001c000, LENGTH =   16K
}

EXTERN(_ZN3etl6armv7mL15exception_tableE)

ENTRY(etl_armv7m_reset_handler)

SECTIONS {
  .vector_tables : {
    PROVIDE(_kernel_rom_start = .);
    KEEP(*(.etl_armv7m_exception_table*))
    KEEP(*(.stm32f4xx_interrupt_redirect))
  } >rom

  .text : ALIGN(4) {
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

  .stack (NOLOAD) : ALIGN(4) {
    . += 512;  /* Leave room for a stack. */
    PROVIDE(etl_armv7m_initial_stack_top = .);
  } >ccm

  .data : ALIGN(4) {
    PROVIDE(_kernel_ram_start = .);
    *(.data*)
  } >ccm AT>rom

  PROVIDE(_data_init_image_start = LOADADDR(.data));
  PROVIDE(_data_start = ADDR(.data));
  PROVIDE(_data_end = ADDR(.data) + SIZEOF(.data));
  PROVIDE(_kernel_rom_end = ALIGN(LOADADDR(.data) + SIZEOF(.data), 4));

  .bss (NOLOAD) : ALIGN(4) {
    PROVIDE(_bss_start = .);
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    PROVIDE(_bss_end = .);
    PROVIDE(_kernel_ram_end = .);
  } >ccm
}
