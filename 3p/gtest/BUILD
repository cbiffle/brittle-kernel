c_library('gtest',
  sources = [ 'upstream/googletest/src/gtest-all.cc' ],
  local = {
    'cxx_flags': [
      '-isystem %(ROOT)s/3p/gtest/upstream/googletest/include',
      '-DGTEST_OS_LINUX',

      # This is here because gtest-all.cc #includes the other source files.
      '-isystem %(ROOT)s/3p/gtest/upstream/googletest',
    ],
  },
  using = {
    'cxx_flags': [
      '-isystem %(ROOT)s/3p/gtest/upstream/googletest/include',
      '-DGTEST_OS_LINUX',
    ],
  },
  deps = [ ':pthread' ],
)

c_library('pthread',
  using = {
    'link_srcs': [ '-lpthread' ],
  },
)
