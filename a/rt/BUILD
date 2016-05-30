c_library('rt',
  sources = [
    'keys.cc',
    'ipc.cc',
    'ipc.S',
  ],
)

c_library('crt0',
  sources = [
    'crt0.cc',
  ],
  using = {
    'link_flags': [
      '-u main',
      '-nostartfiles',
    ],
  },
)
