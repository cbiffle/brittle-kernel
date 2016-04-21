#include <cstdint>

#include "etl/armv7m/mpu.h"
#include "etl/data/range_ptr.h"
#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/exception_table.h"

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

  extern uint32_t _app_rom_start, _app_rom_end;
  extern uint32_t _app_ram0_start, _app_ram0_end;
}

__attribute__((section(".app_info0")))
__attribute__((used))
constexpr AppInfo app_info {
  .abi_token = current_abi_token,

  .memory_map_count = 2,
  .extra_slot_count = 0,
  .external_interrupt_count = 0,

  .donated_ram_begin = reinterpret_cast<uint32_t>(&_donated_ram_begin),
  .donated_ram_end = reinterpret_cast<uint32_t>(&_donated_ram_end),
  .initial_task_sp = reinterpret_cast<uint32_t>(&_demo_initial_stack),
  .initial_task_pc = reinterpret_cast<uint32_t>(&etl_armv7m_reset_handler),

  .initial_task_grants = {
    {  // ROM
      .memory_index = 4,
      .brand = uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_read_u_read)) >> 8,
    },
    {  // RAM
      .memory_index = 5,
      .brand = uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_write_u_write)
          .with_xn(true)) >> 8,
    },
  },

  .memory_map = {},
};

__attribute__((section(".app_info1")))
__attribute__((used))
constexpr AppInfo::MemoryMapEntry memory_map[] {
  {
    // 4: Memory describing application ROM.
    reinterpret_cast<uint32_t>(&_app_rom_start),
    reinterpret_cast<uint32_t>(&_app_rom_end),
  },
  {
    // 5: Memory describing application RAM.
    reinterpret_cast<uint32_t>(&_app_ram0_start),
    reinterpret_cast<uint32_t>(&_app_ram0_end),
  },
};

__attribute__((section(".donated_ram")))
uint8_t kernel_donation[768];

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
