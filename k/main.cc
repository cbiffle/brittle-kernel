#include "etl/prediction.h"

#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/exception_frame.h"
#include "etl/armv7m/exception_table.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/registers.h"
#include "etl/armv7m/scb.h"

#include "k/context.h"

__attribute__((aligned(32)))
static void task_code() {
  while (true);
}

__attribute__((aligned(512)))
static uint8_t task_data[256];

static void setup_mpu() {
  using etl::armv7m::mpu;
  using etl::armv7m::Mpu;

  mpu.write_rbar(Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(&task_code) >> (32-27))
      .with_valid(true)
      .with_region(0));
  mpu.write_rasr(Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_read_u_read)
      .with_tex(0)
      .with_c(true)
      .with_b(true)
      .with_s(false)
      .with_size(4)
      .with_enable(true));

  mpu.write_rbar(Mpu::rbar_value_t()
      .with_addr_27(reinterpret_cast<uintptr_t>(&task_data) >> (32-27))
      .with_valid(true)
      .with_region(1));
  mpu.write_rasr(Mpu::rasr_value_t()
      .with_xn(false)
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_tex(0)
      .with_c(false)
      .with_b(false)
      .with_s(false)
      .with_size(7)
      .with_enable(true));

  mpu.write_ctrl(Mpu::ctrl_value_t()
      .with_privdefena(true)
      .with_hfnmiena(false)
      .with_enable(true));
}

/*
struct Registers : etl::armv7m::ExceptionFrame {
  using Word = etl::armv7m::Word;

  Word r4, r5, r6, r7;
  Word r8, r9, r10, r11;
};
*/

static void prepare_task() {
  auto r = reinterpret_cast<etl::armv7m::ExceptionFrame *>(&task_data[256]) - 1;
  r->psr = 1 << 24;
  r->r15 = reinterpret_cast<uintptr_t>(&task_code) & ~1;
  k::contexts[0].stack = reinterpret_cast<uintptr_t>(r);
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

int main() {
  etl::armv7m::scb.enable_faults();

  setup_mpu();
  prepare_task();
  start_task();
}

static bool booted = false;

void svc_dispatch();
void svc_dispatch() {
  if (ETL_LIKELY(booted)) {
    // Normal invocation.
  } else {
    // First syscall to start initial task.
    // Drop privileges.
    etl::armv7m::set_control(1);
    // Force the stack pointer to the task's.
    etl::armv7m::set_psp(k::current->stack);
    booted = true;
  }
}
