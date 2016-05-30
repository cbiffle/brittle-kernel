MEMORY {
  rom (rx)      : ORIGIN = 0x08000000, LENGTH =  128K
  ccm (rwx)     : ORIGIN = 0x10000000, LENGTH =   64K
  sram128 (rwx) : ORIGIN = 0x20000000, LENGTH =  128K
}

ENTRY(etl_armv7m_reset_handler)

SECTIONS {
  /*
   * ROM: kernel
   */
  .kernel_text : {
    KEEP(*(.__kernel__.vector_tables))
    KEEP(*(.__kernel__.text))
    KEEP(*(.__kernel__.init*))
    KEEP(*(SORT(.app_info*)))
  } >rom

  .kernel_data : {
    KEEP(*(.__kernel__.data))
  } >ccm AT>rom

  /*
   * ROM: system
   */
  .text ORIGIN(rom) + 16K : {
    PROVIDE(_sys_rom_start = .);

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

  /* Align end of system ROM to a pow2 boundary */
  . = ORIGIN(rom) + 32K;
  PROVIDE(_sys_rom_end = .);

  /*
   * RAM: kernel
   */
  .kernel_ram (NOLOAD) : {
    KEEP(*(.__kernel__.stack))
    KEEP(*(.__kernel__.bss))
  } >ccm

  .donated_ram (NOLOAD) : {
    PROVIDE(_donated_ram_begin = .);
    KEEP(*(.donated_ram*))
    PROVIDE(_donated_ram_end = .);
  } >ccm

  /*
   * RAM: system
   */
  .stack : {
    PROVIDE(_sys_ram0_start = .);
    . += 2048; /* TODO right-size */
    PROVIDE(_sys_initial_stack = .);
  } >sram128

  .data : {
    *(.data*)
  } >sram128 AT>rom

  PROVIDE(_data_init_image_start = LOADADDR(.data));
  PROVIDE(_data_start = ADDR(.data));
  PROVIDE(_data_end = ADDR(.data) + SIZEOF(.data));

  .bss (NOLOAD) : ALIGN(4) {
    PROVIDE(_bss_start = .);
    *(.bss*)
    . = ALIGN(4);
    PROVIDE(_bss_end = .);
  } >sram128

  . = ORIGIN(sram128) + 4K;
  PROVIDE(_sys_ram0_end = .);

  .app_ram1 ORIGIN(sram128) + 64K (NOLOAD) : {
    PROVIDE(_app_ram1_start = .);
    . = ORIGIN(sram128) + 128K;
    PROVIDE(_app_ram1_end = .);
  } >sram128
}
