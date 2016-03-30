MEMORY {
  rom (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  ccm (rwx)     : ORIGIN = 0x10000000, LENGTH =   64K
  sram112 (rwx) : ORIGIN = 0x20000000, LENGTH =  112K
  sram16  (rwx) : ORIGIN = 0x2001c000, LENGTH =   16K
}

ENTRY(etl_armv7m_reset_handler)

SECTIONS {
  .kernel_text : {
    KEEP(*(.__kernel__.vector_tables))
    KEEP(*(.__kernel__.text))
    KEEP(*(.__kernel__.init*))
    KEEP(*(SORT(.app_info*)))
  } >rom

  .kernel_data : {
    KEEP(*(.__kernel__.data))
  } >ccm AT>rom

  .text ORIGIN(rom) + 16384 : {
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

  PROVIDE(_app_rom_start = ADDR(.text));
  PROVIDE(_app_rom_end = ORIGIN(rom) + LENGTH(rom));

  .kernel_ram : {
    KEEP(*(.__kernel__.stack))
    KEEP(*(.__kernel__.bss))
  } >ccm

  PROVIDE(_donated_ram_begin = ADDR(.kernel_ram) + SIZEOF(.kernel_ram));
  PROVIDE(_donated_ram_end = ORIGIN(ccm) + LENGTH(ccm));

  .stack : {
    . += 512;
    PROVIDE(_demo_initial_stack = .);
  } >sram112

  .data : {
    *(.data*)
  } >sram112 AT>rom

  PROVIDE(_data_init_image_start = LOADADDR(.data));
  PROVIDE(_data_start = ADDR(.data));
  PROVIDE(_data_end = ADDR(.data) + SIZEOF(.data));

  .bss (NOLOAD) : ALIGN(4) {
    PROVIDE(_bss_start = .);
    *(.bss*)
    . = ALIGN(4);
    PROVIDE(_bss_end = .);
  } >sram112

  PROVIDE(_app_ram_start = ORIGIN(sram112));
  PROVIDE(_app_ram_end = ORIGIN(sram112) + LENGTH(sram112));
}
