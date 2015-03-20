c_library('test',
  sources = [
    'mock.cc',
    'task.cc',
  ],
  deps = [
    '//3p/gtest',
  ],
)

c_library('stubs',
  sources = [
    'stubs.cc',
  ],
)

c_binary('child',
  environment = 'native',
  sources = [
    'child.cc',
  ],
  deps = [
    ':stubs',
  ],
)

c_binary('parent',
  environment = 'native',
  sources = [
    'parent.cc',
  ],
  deps = [
    ':test',
    '//3p/gtest',
  ],
)

