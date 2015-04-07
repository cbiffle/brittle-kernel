#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/exception_table.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/registers.h"
#include "etl/armv7m/scb.h"

#include "k/context.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/registers.h"
#include "k/zoo.h"

static void setup_mpu() {
  using etl::armv7m::mpu;
  using etl::armv7m::Mpu;

  mpu.write_ctrl(Mpu::ctrl_value_t()
      .with_privdefena(true)
      .with_hfnmiena(false)
      .with_enable(true));
}

static void prepare_task() {
  using etl::armv7m::Mpu;

  auto & ctx = k::contexts[0];

  // Enable initial ROM grant.
  auto rom_key = k::init_rom.make_key(k::init_rom_brand);
  ETL_ASSERT(rom_key);  // Failure indicates misconfiguration.
  ctx.memory_region(0) = rom_key.ref();

  // Enable initial RAM grant.
  auto ram_key = k::init_ram.make_key(k::init_ram_brand);
  ETL_ASSERT(ram_key);  // Failure indicates misconfiguration.
  ctx.memory_region(1) = ram_key.ref();

  auto rom_begin = k::init_rom.get_region_begin(k::init_rom_brand);
  auto ram_end   = k::init_ram.get_region_end(k::init_ram_brand);
  auto r = static_cast<k::StackRegisters *>(ram_end) - 1;
  r->psr = 1 << 24;
  r->r15 = reinterpret_cast<uintptr_t>(rom_begin);
  ctx.set_stack(r);

  // Provide the context with its initial authority (in the form of a single
  // key to the object table).
  ctx.key(4).fill(1, 0);
}

__attribute__((noreturn))
static void start_task() {
  k::current = &k::contexts[0];
  k::current->apply_to_mpu();

  asm volatile (
      "mov r0, sp\n"        // Get current stack pointer and
      "msr PSP, r0\n"       // replicate it in process stack pointer.
      "msr MSP, %0\n"       // Reset main stack pointer.
      "mov r0, #2\n"
      "msr CONTROL, r0\n"   // Drop privs, switch stacks.
      "svc #0"
      :: "r"(&etl_armv7m_initial_stack_top)
      );
  __builtin_unreachable();
}

int main() {
  etl::armv7m::scb.enable_faults();

  k::init_zoo();
  setup_mpu();
  prepare_task();
  start_task();
}
