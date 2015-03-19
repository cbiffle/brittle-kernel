c_binary('child',
  environment = 'native',
  sources = [
    'child.cc',
  ],
)

c_binary('parent',
  environment = 'native',
  sources = [
    'parent.cc',
  ],
  deps = [
    '//3p/gtest',
  ],
)

