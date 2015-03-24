#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/exception_table.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/registers.h"
#include "etl/armv7m/scb.h"

#include "k/context.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/registers.h"

namespace demo {
  extern void main();
}

extern uint8_t _demo_data_start, _demo_stack;

static void setup_mpu() {
  using etl::armv7m::mpu;
  using etl::armv7m::Mpu;

  mpu.write_rbar(Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(demo::main) >> (32-27))
      .with_valid(true)
      .with_region(0));
  mpu.write_rasr(Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_read_u_read)
      .with_tex(0)
      .with_c(true)
      .with_b(true)
      .with_s(false)
      .with_size(9)
      .with_enable(true));

  mpu.write_rbar(Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(&_demo_data_start) >> (32-27))
      .with_valid(true)
      .with_region(1));
  mpu.write_rasr(Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_tex(0)
      .with_c(false)
      .with_b(false)
      .with_s(false)
      .with_size(10)
      .with_enable(true));

  mpu.write_ctrl(Mpu::ctrl_value_t()
      .with_privdefena(true)
      .with_hfnmiena(false)
      .with_enable(true));
}

static void prepare_task() {
  auto r = reinterpret_cast<k::Registers *>(&_demo_stack) - 1;
  r->ef.psr = 1 << 24;
  r->ef.r15 = reinterpret_cast<uintptr_t>(demo::main);
  k::contexts[0].set_stack(r);

  k::contexts[0].key(0).fill(1, 0);

  k::current = &k::contexts[0];
}

__attribute__((noreturn))
static void start_task() {
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

static k::NullObject null;

static void setup_objects() {
  auto constexpr special_objects = 2;
  k::object_table[0].ptr = &null;
  k::object_table[1].ptr = &k::object_table;

  static_assert(k::config::n_objects >= k::config::n_contexts + special_objects,
                "Not enough object table entries to hold all objects.");
  for (unsigned i = 0; i < k::config::n_contexts; ++i) {
    k::object_table[i + special_objects].ptr = &k::contexts[i];
  }
}

int main() {
  etl::armv7m::scb.enable_faults();

  setup_objects();
  setup_mpu();
  prepare_task();
  start_task();
}
