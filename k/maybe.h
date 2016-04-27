#ifndef K_MAYBE_H
#define K_MAYBE_H

#include "etl/data/maybe.h"

#include "k/panic.h"

namespace k {

/*
 * Check policy for Maybes used in the kernel; converts any misuse into a
 * kernel panic.
 */
struct PanicMaybeCheckPolicy {
  static constexpr bool check_access(bool condition) {
    return PANIC_UNLESS(condition, "empty Maybe accessed"), true;
  }
};

/*
 * Import the ETL Maybe type into our namespace, specialized with our checking
 * policy.
 */
template <typename T>
using Maybe = etl::data::Maybe<T, PanicMaybeCheckPolicy>;

constexpr auto nothing = etl::data::nothing;

}  // namespace k

#endif  // K_MAYBE_H
