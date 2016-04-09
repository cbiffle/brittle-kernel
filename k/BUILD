c_library('k_portable',
  sources = [
    'address_range.cc',
    'key.cc',
    'object_table.cc',
    'reply_sender.cc',
  ],
  local = {
    'cxx_flags': ['-Wno-invalid-offsetof'],
  },
).extend_when(lambda e: e['arch'] == 'target',
  sources = [
    'object.cc',
  ],
  deps = [
    '//etl:assert_loop'
  ],
).extend_when(lambda e: e['arch'] == 'native',
  sources = [
    'object_native.cc',
  ],
  deps = [
    ':assert_fail_test',
    '//3p/gtest',
  ],
)

c_library('k',
  sources = [
    'app.cc',
    'context.cc',
    'cxxabi.cc',
    'entry.S',
    'gate.cc',
    'interrupt.cc',
    'irq_redirector.cc',
    'main.cc',
    'mm_fault.cc',
    'null_object.cc',
    'traps.cc',
    'reply_gate.cc',
    'scheduler.cc',
    'sys_tick.cc',
    'unprivileged.cc',
    'unprivileged.S',
  ],
  deps = [
    ':k_portable',
    '//etl:assert_loop'
  ],
  local = {
    'cxx_flags': ['-Wno-invalid-offsetof'],
  },
)

c_binary('address_range_test',
  environment = 'native',
  sources = [
    'address_range_test.cc',
  ],
  deps = [
    ':k_portable',
    '//3p/gtest',
  ],
)

c_library('assert_fail_test',
  sources = [
    'assert_fail_test.cc',
  ],
  deps = [
    '//3p/gtest',
  ],
)

c_library('stm32f4xx',
  sources = [
    'stm32f4xx_irq.cc',
  ],
  deps = [
    ':k',
    '//etl/stm32f4xx',
  ],
  local = {
    'whole_archive': True,
  },
)

