#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/scb.h"

#include "k/app.h"

static void setup_mpu() {
  using etl::armv7m::mpu;
  using etl::armv7m::Mpu;

  mpu.write_ctrl(Mpu::ctrl_value_t()
      .with_privdefena(true)
      .with_hfnmiena(false)
      .with_enable(true));
}

int main() {
  etl::armv7m::scb.enable_faults();

  setup_mpu();
  k::start_app();
}
