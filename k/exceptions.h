#ifndef K_EXCEPTIONS_H
#define K_EXCEPTIONS_H

#include <cstdint>

namespace k {

/*
 * Types of exceptions known to the kernel.
 */
enum class Exception : uint64_t {
  // Reasonably general exceptions.
  bad_operation = 0xfbfee62145593586,
  would_block = 0x1dfc663bc104def5,
  fault = 0x07b819f85884ac2e,
  index_out_of_range = 0x0aa90fb0c1751680,
  bad_syscall = 0xabece95f618e09ea,

  // Exceptions produced by the Object Table.
  bad_brand = 0x71decab8530ce0eb,
};

}  // namespace k

#endif  // K_EXCEPTIONS_H
