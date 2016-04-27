#ifndef K_RANGE_PTR_H
#define K_RANGE_PTR_H

#include "etl/data/range_ptr.h"

#include "k/panic.h"

namespace k {

/*
 * Range access checking policy for RangePtrs used in the kernel.  This
 * converts any misuse into a kernel panic.
 */
struct PanicRangeCheckPolicy {
  static constexpr std::size_t check_index(std::size_t index,
      std::size_t count) {
    return PANIC_UNLESS(index < count, "index out of range"), index;
  }

  static constexpr std::size_t check_slice_start(std::size_t start,
      std::size_t end,
      std::size_t count) {
    return PANIC_UNLESS(start <= count, "slice start out of range"), start;
  }

  static constexpr std::size_t check_slice_end(std::size_t start,
      std::size_t end,
      std::size_t count) {
    return PANIC_UNLESS(start <= end && end <= count, "slice end out of range"),
           end - start;
  }
};

/*
 * Import the ETL RangePtr type into our namespace, specialized for our check
 * policy.
 */
template <typename T>
using RangePtr = etl::data::RangePtr<T, PanicRangeCheckPolicy>;

}  // namespace k

#endif  // K_RANGE_PTR_H
