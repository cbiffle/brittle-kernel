c_binary('kernel',
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
    'klinker_script': '%(ROOT)s/demo/kernel-stm32f4xx.ld',
  },
  local = {
    'link_flags': [
      '-T%(klinker_script)s',
    ],
    '__implicit__': [ '%(klinker_script)s' ],
  },
)

c_binary('demo',
  environment = 'stm32f407',
  deps = [
    ':code',
    '//etl/stm32f4xx',
  ],
  extra = {
    'cxx_flags': [
      '-ffreestanding',
      '-nostdlib',
      '-nodefaultlibs',
    ],
  },
  local = {
    'link_flags': [
      '-T%(linker_script)s',
    ],
    '__implicit__': [ '%(linker_script)s' ],
  },
)

c_library('code',
  sources = [
    'demo.cc',
  ],
  deps = [
    '//demo/client',
    '//demo/driver',
    '//demo/k',
    '//demo/runtime',
  ],
)
