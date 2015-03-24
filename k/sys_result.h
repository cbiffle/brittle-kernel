#ifndef K_SYS_RESULT_H
#define K_SYS_RESULT_H

#include <stdint.h>

namespace k {

enum class SysResult : uint32_t {
  success = 0,

  bad_key_index,
  bad_svc,
  fault,
  bad_key,

  bad_message,
};

}  // namespace k

#endif  // K_SYS_RESULT_H
