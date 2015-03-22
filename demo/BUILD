c_binary('demo',
  environment = 'stm32f407',
  deps = [
    '//k',
    '//etl/armv7m:exception_table',
    '//etl/stm32f4xx:interrupt_table',
    '//etl/armv7m:implicit_crt0',
  ],
  extra = {
    'cxx_flags': [
      '-ffreestanding',
    ],
  },
  local = {
    'link_flags': [
      '-T%(linker_script)s',
    ],
    '__implicit__': [ '%(linker_script)s' ],
  },
)
