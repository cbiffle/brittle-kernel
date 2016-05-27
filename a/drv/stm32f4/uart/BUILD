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
