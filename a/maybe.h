#ifndef A_MAYBE_H
#define A_MAYBE_H

#include "etl/data/maybe.h"

/*
 * Import the ETL Maybe type into our namespace, specialized with our checking
 * policy.
 */
template <typename T>
using Maybe = etl::data::Maybe<T, etl::data::AssertMaybeCheckPolicy>;

constexpr auto nothing = etl::data::nothing;

#endif  // A_MAYBE_H
