c_library('k_portable',
  sources = [
    'context.cc',
    'gate.cc',
    'interrupt.cc',
    'irq_redirector.cc',
    'key.cc',
    'memory.cc',
    'null_object.cc',
    'object_table.cc',
    'reply_gate.cc',
    'reply_sender.cc',
    'scheduler.cc',
    'slot.cc',
    'sys_tick.cc',
  ],
  local = {
    'cxx_flags': ['-Wno-invalid-offsetof'],
  },
).extend_when(lambda e: e['arch'] == 'target',
  sources = [
    'object.cc',
    'unprivileged.cc',
    'unprivileged.S',
  ],
  deps = [
    '//etl:assert_loop'
  ],
).extend_when(lambda e: e['arch'] == 'native',
  sources = [
    'testutil/mpu_fake.cc',
    'testutil/object_native.cc',
    'testutil/sys_tick_fake.cc',
    'testutil/unprivileged_fake.cc',
  ],
  deps = [
    ':assert_fail_test',
    '//3p/gtest',
  ],
)

c_library('k',
  sources = [
    'app.cc',
    'cxxabi.cc',
    'entry.S',
    'irq_entry.cc',
    'main.cc',
    'mm_fault.cc',
    'traps.cc',
  ],
  deps = [
    ':k_portable',
    '//etl:assert_loop'
  ],
  local = {
    'cxx_flags': ['-Wno-invalid-offsetof'],
  },
)

c_binary('memory_test',
  environment = 'native',
  sources = [
    'memory_test.cc',
  ],
  deps = [
    ':k_portable',
    ':spy',
    '//3p/gtest',
  ],
)

c_binary('p2range_test',
  environment = 'native',
  sources = [
    'p2range_test.cc',
  ],
  deps = [
    ':k_portable',
  ],
)

c_library('assert_fail_test',
  sources = [
    'testutil/assert_fail_test.cc',
  ],
  deps = [
    '//3p/gtest',
  ],
)

c_library('spy',
  sources = [
    'testutil/spy.cc',
  ],
  deps = [
    ':k_portable',
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

