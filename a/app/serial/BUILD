c_library('setup',
  sources = [ 'setup.cc' ],
  deps = [
    '//a/sys:core',
    '//a/drv/stm32f4/uart:uart.img',
    '//a/srv/ascii:ascii.img',
  ],
)

c_binary('serial',
  environment = 'stm32f407_app',
  deps = [
    '//a/sys',
    '//a/rt',
    '//a/k',

    '//demo:kernel.blob',
    '//etl:assert_loop',
    '//etl/armv7m:implicit_crt0',

  ],
  extra = {
    'peanut_setup_target': '//a/app/serial:setup',
    'peanut_config_dir': '%(ROOT)s/a/app/serial',
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


