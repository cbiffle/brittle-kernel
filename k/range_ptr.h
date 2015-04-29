#ifndef K_RANGE_PTR_H
#define K_RANGE_PTR_H

#include "etl/type_traits.h"
#include "etl/data/range_ptr.h"

#include "k/config.h"

namespace k {

/*
 * Import the ETL RangePtr type into our namespace, with configurable runtime
 * checking.
 */
template <typename T>
using RangePtr = etl::data::RangePtr<T,
      etl::ConditionalT<config::checks, etl::data::AssertRangeCheckPolicy,
                                        etl::data::LaxRangeCheckPolicy>>;

}  // namespace k

#endif  // K_RANGE_PTR_H
