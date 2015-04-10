c_library('k_portable',
  sources = [
    'address_range.cc',
    'key.cc',
    'object_table.cc',
    'reply_sender.cc',
    'sender.cc',
  ],
  deps = [
    '//etl:assert_loop'
  ],
  local = {
    'cxx_flags': ['-Wno-invalid-offsetof'],
  },
).extend_when(lambda e: e['arch'] == 'target',
  sources = [
    'object.cc',
  ],
).extend_when(lambda e: e['arch'] == 'native',
  sources = [
    'object_native.cc',
  ],
)

c_library('k',
  sources = [
    'context.cc',
    'cxxabi.cc',
    'entry.S',
    'gate.cc',
    'main.cc',
    'mm_fault.cc',
    'null_object.cc',
    'traps.cc',
    'reply_gate.cc',
    'svc.cc',
    'unprivileged.cc',
    'unprivileged.S',
    'zoo.cc',
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

