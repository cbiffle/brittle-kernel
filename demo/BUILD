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

c_binary('demo',
  environment = 'stm32f407_app',
  deps = [
    ':code',
    ':kernel.blob',
    '//etl/stm32f4xx',
    '//etl/armv7m:implicit_crt0',
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
    'link_srcs': [
      'gen/demo/kernel.blob',
    ],
    '__implicit__': [ '%(linker_script)s' ],
  },
)

c_library('code',
  sources = [
    'demo.cc',
  ],
  deps = [
    '//etl:assert_loop',
    '//demo/client',
    '//demo/driver',
    '//demo/k',
    '//demo/runtime',
  ],
)
