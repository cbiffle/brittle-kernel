#include "k/memory_map.h"

#include "etl/armv7m/mpu.h"

namespace k {

bool is_unprivileged_access_ok(void * begin, void * end) {
  using etl::armv7m::mpu;

  if (begin <= (&mpu + 1) && &mpu <= end) return false;
  return true;
}

}  // namespace k
