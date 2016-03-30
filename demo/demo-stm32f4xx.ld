MEMORY {
  rom (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  ccm (rwx)     : ORIGIN = 0x10000000, LENGTH =   64K
  sram112 (rwx) : ORIGIN = 0x20000000, LENGTH =  112K
  sram16  (rwx) : ORIGIN = 0x2001c000, LENGTH =   16K
}

ENTRY(_ZN4demo4mainEv)

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
    KEEP(*(.text.startup._ZN4demo4mainEv))
    *(.text*)
    . = ALIGN(4);
    *(.rodata*)
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
    PROVIDE(_demo_data_start = .);
    *(.data*)
  } >sram112 AT>rom

  .bss (NOLOAD) : ALIGN(4) {
    PROVIDE(_demo_bss_start = .);
    *(.bss*)
    . = ALIGN(4);
    PROVIDE(_demo_bss_end = .);
  } >sram112

  PROVIDE(_app_ram_start = ORIGIN(sram112));
  PROVIDE(_app_ram_end = ORIGIN(sram112) + LENGTH(sram112));
}
