c_binary('uart.elf',
  environment = 'stm32f407_prog',
  sources = [ 'main.cc' ],
  deps = [
    '//etl/stm32f4xx',
    '//etl:assert_loop',
    '//a/rt',
    '//a/k',
    '//a/rt:crt0',
    '//a/sys:api',
  ],
  local = {
    'link_flags': [
      '-T%(linker_script)s',
      '-Wl,--defsym=_stack_size=256',
    ],
    '__implicit__': [ '%(linker_script)s' ],
  },
)

objcopy('uart.bin',
  environment = 'stm32f407_prog',
  src = 'latest/a/drv/stm32f4/uart/uart.elf',
  options = [
    '--strip-all',
    '--output-target=binary',
  ],
)

c_library('uart.img',
  sources = [ 'uart-bin.S' ],
  deps = [ ':uart.bin' ],
)
