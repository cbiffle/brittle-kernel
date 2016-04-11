c_library('code',
  sources = [
    'client.cc',
  ],
  deps = [
    '//demo/runtime',
    '//demo/driver:api',
  ],
)

c_binary('demo',
  environment = 'stm32f407_app',
  sources = [
    'main.cc',
  ],
  deps = [
    ':code',
    '//demo:kernel.blob',
    '//demo/driver',
    '//demo/k',
    '//demo/runtime',
    '//etl:assert_loop',
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


