#include <cstdint>

#include "etl/armv7m/mpu.h"
#include "etl/data/range_ptr.h"
#include "etl/armv7m/implicit_crt0.h"

#include "common/app_info.h"

#include "demo/runtime/ipc.h"

using etl::data::RangePtr;
using etl::armv7m::Mpu;
using Rbar = Mpu::rbar_value_t;
using Rasr = Mpu::rasr_value_t;

namespace demo {

extern "C" {
  extern uint32_t _donated_ram_begin, _donated_ram_end;
  extern uint32_t _demo_initial_stack;

  extern uint32_t _app_rom_start, _app_rom_l2_half_size;
  extern uint32_t _app_ram_start, _app_ram_l2_half_size;
}

__attribute__((section(".app_info0")))
__attribute__((used))
constexpr AppInfo app_info {
  .abi_token = current_abi_token,

  .object_table_entry_count = 6,
  .external_interrupt_count = 0,

  .donated_ram_begin = reinterpret_cast<uint32_t>(&_donated_ram_begin),
  .donated_ram_end = reinterpret_cast<uint32_t>(&_donated_ram_end),
  .initial_task_sp = reinterpret_cast<uint32_t>(&_demo_initial_stack),
  .initial_task_pc = reinterpret_cast<uint32_t>(&main),

  .initial_task_grants = {
    {  // ROM
      .memory_index = 4,
      .brand_lo = uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_read_u_read)) >> 8,
    },
    {  // RAM
      .memory_index = 5,
      .brand_lo = uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_write_u_write)
          .with_xn(true)) >> 8,
    },
  },

  .object_map = {},
};

__attribute__((section(".app_info1")))
__attribute__((used))
constexpr uint32_t object_map[] {
  // 4: Memory describing application ROM.
  0,  // address range
  reinterpret_cast<uint32_t>(&_app_rom_start),  // begin
  reinterpret_cast<uint32_t>(&_app_rom_l2_half_size),
  // 5: Memory describing application RAM.
  0,  // address range
  reinterpret_cast<uint32_t>(&_app_ram_start),  // begin
  reinterpret_cast<uint32_t>(&_app_ram_l2_half_size),
};


static constexpr unsigned
  oi_object_table = 1;

static constexpr unsigned
  k_object_table = 4;

__attribute__((noreturn))
static void demo_main() {
  while (true) {
    ipc({
        Descriptor::call(0, k_object_table),
        oi_object_table,
        0,
        0,
        });
  }
}

}  // namespace demo

int main() {
  demo::demo_main();
}
