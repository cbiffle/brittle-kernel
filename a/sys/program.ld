MEMORY {
  flatmem : ORIGIN = 0, LENGTH = 4096M
}

ENTRY(_start)

PHDRS {
  text PT_LOAD;
  data PT_LOAD;
}

SECTIONS {
  .text 0 : {
    LONG(_data_end)
    LONG(_text_end)
    LONG(_got_end)
    LONG(_bss_end)
    LONG(_stack_size)
    LONG(_start)
    *(.text*)
    . = ALIGN(4);
    *(.rodata*)
  } >flatmem :text

  .init : ALIGN(2) {
    KEEP(*(.init_prologue))
    KEEP(*(.init))
    KEEP(*(.init_epilogue))
  } >flatmem

  .preinit_array : ALIGN(4) {
    _preinit_array_start = .;
    KEEP(*(.preinit_array))
    _preinit_array_end = .;
  } >flatmem

  .init_array : ALIGN(4) {
    _init_array_start = .;
    KEEP(*(.init_array))
    _init_array_end = .;
    . = ALIGN(4);
    PROVIDE(_text_end = .);
  } >flatmem


  .data : {
    _got_start = .;
    *(.got.plt)
    *(.got)
    _got_end = .;
    . = ALIGN(4);
    *(.data*)
    . = ALIGN(4);
    PROVIDE(_data_end = .);
  } >flatmem :data

  .bss (NOLOAD) : ALIGN(4) {
    PROVIDE(_bss_start = .);
    *(.bss*)
    . = ALIGN(4);
    PROVIDE(_bss_end = .);
  } >flatmem
}
