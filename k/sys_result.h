#ifndef K_SYS_RESULT_H
#define K_SYS_RESULT_H

#include <stdint.h>

#include "etl/error/result.h"
#include "etl/error/strategy.h"

namespace k {

enum class SysResult : uint32_t {
  success = 0,

  bad_key_index,
  bad_svc,
  fault,
  bad_key,

  bad_message,
};

template<typename V>
using SysResultWith = etl::error::Result<SysResult, V>;

}  // namespace k

namespace etl {
namespace error {

template <>
struct Strategy<::k::SysResult>
  : public TraditionalEnumStrategy<::k::SysResult, ::k::SysResult::success> {};

}  // namespace error
}  // namespace etl

#endif  // K_SYS_RESULT_H
