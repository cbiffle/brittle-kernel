#include <cstdint>

#include "etl/array_count.h"
#include "etl/armv7m/mpu.h"
#include "etl/data/range_ptr.h"
#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/exception_table.h"

#include "common/app_info.h"
#include "common/abi_sizes.h"

#include "a/rt/ipc.h"
#include "a/rt/keys.h"

#include "a/k/context.h"
#include "a/k/memory.h"
#include "a/k/object_table.h"

#include "a/sys/keys.h"
#include "a/sys/alloc.h"
#include "a/sys/idle.h"

#include "peanut_config.h"

using etl::data::RangePtr;
using etl::armv7m::Mpu;
using Rbar = Mpu::rbar_value_t;
using Rasr = Mpu::rasr_value_t;

namespace sys {
namespace sketch {

extern "C" {
  extern uint32_t _donated_ram_begin, _donated_ram_end;
  extern uint32_t _sys_initial_stack;

  extern uint32_t _sys_rom_start, _sys_rom_end;
  extern uint32_t _sys_ram0_start, _sys_ram0_end;
  extern uint32_t _app_ram1_start, _app_ram1_end;
}

__attribute__((section(".app_info0")))
__attribute__((used))
constexpr AppInfo app_info {
  .abi_token = current_abi_token,

  .memory_map_count = config::memory_map_count,
  .device_map_count = 0,
  .extra_slot_count = config::extra_slot_count,
  .external_interrupt_count = 0,

  .donated_ram_begin = reinterpret_cast<uint32_t>(&_donated_ram_begin),
  .donated_ram_end = reinterpret_cast<uint32_t>(&_donated_ram_end),
  .initial_task_sp = reinterpret_cast<uint32_t>(&_sys_initial_stack),
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
    reinterpret_cast<uint32_t>(&_sys_rom_start),
    reinterpret_cast<uint32_t>(&_sys_rom_end),
  },
  {
    // 5: Memory describing system ROM.
    reinterpret_cast<uint32_t>(&_sys_ram0_start),
    reinterpret_cast<uint32_t>(&_sys_ram0_end),
  },
  {
    // 6: Memory describing application RAM.
    reinterpret_cast<uint32_t>(&_app_ram1_start),
    reinterpret_cast<uint32_t>(&_app_ram1_end),
  },
};

__attribute__((section(".donated_ram")))
uint8_t kernel_donation[2048];

static constexpr unsigned
  oi_object_table = 1,
  oi_first_ctx = 2,
  oi_app_ram = 6;

static void feed_allocator() {
  // Mint a key to the application RAM object described above.
  auto k_app_ram = object_table::mint_key(ki::ot, oi_app_ram, 0);
  // Hand said object over to the allocator.
  bool freed = free_mem(k_app_ram);
  // If the allocator doesn't take it, we have a bug!
  ETL_ASSERT(freed);
}

static void autoauth() {
  // Get a context key to ourselves.
  auto k_self = object_table::mint_key(ki::ot, oi_first_ctx, 0);
  // Put it in the appointed place.
  rt::copy_key(ki::self, k_self);
}

static void make_idle_task() {
  // Allocate space for a Context.
  auto k_ctx = etl::move(alloc_mem(kabi::context_l2_size - 1).ref());

  // Make it into a real Context.
  memory::become(k_ctx, memory::ObjectType::context, 0);

  prepare_idle_task(k_ctx);

  // Make it runnable so we can block.
  context::make_runnable(k_ctx);
}

__attribute__((noreturn))
static void main() {
  rt::reserve_key(ki::ot);
  rt::reserve_key(ki::self);

  feed_allocator();
  autoauth();
  make_idle_task();

  while (true);  // TODO do stuff
}

}  // namespace sketch
}  // namespace sys

// Entry point expected by ETL CRT0
int main() {
  sys::sketch::main();
}
