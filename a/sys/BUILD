c_library('sys',
  sources = [
    'main.cc',
  ],
  deps = [
    '%(peanut_setup_target)s',
    ':core',
    '//a/rt',
    '//a/k',
  ],
  local = {
    'cxx_flags': [
      '-I %(peanut_config_dir)s',
    ],
  },
)

c_library('core',
  sources = [
    'alloc.cc',
    'idle.cc',
    'load.cc',
  ],
  deps = [
    '//a/rt',
    '//a/k',
  ],
  local = {
    'cxx_flags': [
      '-I %(peanut_config_dir)s',
    ],
  },
)

c_library('api',
  sources = [ 'api.cc' ],
  deps = [
    '//a/rt',
  ],
)
