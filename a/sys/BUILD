c_library('sys',
  sources = [
    'alloc.cc',
    'idle.cc',
    'load.cc',
  ],
  deps = [
    '//a/k',
  ],
)

c_library('api',
  sources = [ 'api.cc' ],
  deps = [
    '//a/rt',
  ],
)

c_binary('sketch',
  environment = 'stm32f407_app',
  sources = [
    'main.cc',
  ],
  deps = [
    ':sys',

    '//a/rt',
    '//a/k',

    '//demo:kernel.blob',
    '//etl:assert_loop',
    '//etl/armv7m:implicit_crt0',
  ],
  extra = {
    'cxx_flags': [
      '-I%(ROOT)s/a/sys',  # for peanut_config.h
    ],
  },
  local = {
    'link_flags': [
      '-T%(ROOT)s/a/sys/sketch.ld',
    ],
    'link_srcs': [
      'gen/demo/kernel.blob',
    ],
    '__implicit__': [ '%(ROOT)s/a/sys/sketch.ld' ],
  },
)


