c_binary('demo',
  environment = 'stm32f407_app',
  sources = [
    'main.cc',
  ],
  deps = [
    '//demo:kernel.blob',
    '//demo/runtime',
    '//etl:assert_loop',
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


