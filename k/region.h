#ifndef K_REGION_H
#define K_REGION_H

#include "etl/armv7m/mpu.h"

namespace k {

struct Region {
  using Rbar = etl::armv7m::Mpu::rbar_value_t;
  using Rasr = etl::armv7m::Mpu::rasr_value_t;

  Rbar rbar;
  Rasr rasr;
};

}  // namespace k

#endif  // K_REGION_H
