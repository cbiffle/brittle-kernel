c_binary('ascii.elf',
  environment = 'stm32f407_prog',
  sources = [ 'main.cc' ],
  deps = [
    '//etl/stm32f4xx',
    '//a/lib:assert_exit',
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
  extra = {
    'link_flags': [ '-flto' ],
  },
)

objcopy('ascii.bin',
  environment = 'stm32f407_prog',
  src = 'latest/a/srv/ascii/ascii.elf',
  options = [
    '--strip-all',
    '--output-target=binary',
  ],
)

c_library('ascii.img',
  sources = [ 'img.S' ],
  deps = [ ':ascii.bin' ],
)
