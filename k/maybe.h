#ifndef K_MAYBE_H
#define K_MAYBE_H

#include "etl/type_traits.h"
#include "etl/data/maybe.h"

#include "k/config.h"

namespace k {

/*
 * Import the ETL Maybe type into our namespace, with configurable runtime
 * checking.
 */
template <typename T>
using Maybe = etl::data::Maybe<T,
      etl::ConditionalT<config::checks, etl::data::AssertMaybeCheckPolicy,
                                        etl::data::LaxMaybeCheckPolicy>>;

constexpr auto nothing = etl::data::nothing;

}  // namespace k

#endif  // K_MAYBE_H
