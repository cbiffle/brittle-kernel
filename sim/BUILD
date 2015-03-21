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

c_binary('uart',
  environment = 'native',
  sources = [
    'uart.cc',
  ],
  deps = [
    ':stubs',
  ],
)

c_binary('uart_test',
  environment = 'native',
  sources = [
    'uart_test.cc',
  ],
  deps = [
    ':test',
    '//3p/gtest',
  ],
)

