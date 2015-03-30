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

extern uint8_t _app_ram_start, _app_base, _app_stack;
extern uint8_t _kernel_rom_start, _kernel_ram_start;

static void setup_mpu() {
  using etl::armv7m::mpu;
  using etl::armv7m::Mpu;

  /*
   * Use regions 7 and 6 to mask off kernel ROM and RAM, respectively.
   *
   * This limits tasks to 6 regions, but simplifies validation, as these
   * regions are the highest priority ones and can't be overridden.
   */
  mpu.write_rbar(Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(&_kernel_rom_start) >> (32-27))
      .with_valid(true)
      .with_region(7));

  mpu.write_rasr(Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_read_u_none)
      .with_tex(0)
      .with_c(true)
      .with_b(true)
      .with_s(false)
      .with_size(12)
      .with_enable(true));

  mpu.write_rbar(Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(&_kernel_ram_start) >> (32-27))
      .with_valid(true)
      .with_region(6));

  mpu.write_rasr(Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_write_u_none)
      .with_tex(0)
      .with_c(true)
      .with_b(true)
      .with_s(false)
      .with_size(15)
      .with_enable(true));

  mpu.write_ctrl(Mpu::ctrl_value_t()
      .with_privdefena(true)
      .with_hfnmiena(false)
      .with_enable(true));
}

static void prepare_task() {
  using etl::armv7m::Mpu;

  auto r = reinterpret_cast<k::Registers *>(&_app_stack) - 1;
  r->psr = 1 << 24;
  r->r15 = reinterpret_cast<uintptr_t>(&_app_base);
  k::contexts[0].set_stack(r);

  // Enable all of ROM.
  k::contexts[0].region(0) = {
    .rbar = Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(&_kernel_rom_start) >> (32-27)),
    .rasr = Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_read_u_read)
      .with_tex(0)
      .with_c(true)
      .with_b(true)
      .with_s(false)
      .with_size(19)
      .with_enable(true),
  };

  // Enable all of RAM.
  k::contexts[0].region(1) = {
    .rbar = Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(&_app_ram_start) >> (32-27)),
    .rasr = Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_tex(0)
      .with_c(false)
      .with_b(false)
      .with_s(false)
      .with_size(16)
      .with_enable(true),
  };


  k::contexts[0].key(4).fill(1, 0);
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
