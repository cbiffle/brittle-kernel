c_binary('kernel',
  environment = 'stm32f407_kernel',
  deps = [
    '//k',
    '//k:stm32f4xx',
    '//etl/armv7m:exception_table',
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

objcopy('kernel.blob',
  environment = 'stm32f407_app',
  src = 'latest/demo/kernel',
  options = [
    '--prefix-alloc-sections=.__kernel__',
    '-S',
  ],
)
