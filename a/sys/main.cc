#include <cstdint>

#include "etl/armv7m/mpu.h"
#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/exception_table.h"

#include "etl/stm32f4xx/interrupts.h"

#include "common/app_info.h"
#include "common/abi_sizes.h"

#include "a/rt/ipc.h"
#include "a/rt/keys.h"

#include "a/k/context.h"
#include "a/k/memory.h"
#include "a/k/object_table.h"
#include "a/k/interrupt.h"

#include "a/sys/setup.h"
#include "a/sys/keys.h"
#include "a/sys/alloc.h"
#include "a/sys/load.h"
#include "a/sys/idle.h"
#include "a/sys/selectors.h"

#include "peanut_config.h"

using etl::armv7m::Mpu;
using Rasr = Mpu::rasr_value_t;

namespace sys {
namespace sketch {

/*******************************************************************************
 * AppInfo, kernel interfacing.
 */

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
  .device_map_count = config::device_map_count,
  .extra_slot_count = config::extra_slot_count,
  .external_interrupt_count = config::external_interrupt_count,

  .donated_ram_begin = reinterpret_cast<uint32_t>(&_donated_ram_begin),
  .donated_ram_end = reinterpret_cast<uint32_t>(&_donated_ram_end),
  .initial_task_sp = reinterpret_cast<uint32_t>(&_sys_initial_stack),
  .initial_task_pc = reinterpret_cast<uint32_t>(&etl_armv7m_reset_handler),

  .initial_task_grants = {
    {  // ROM
      .memory_index = kabi::well_known_object_count + 0,
      .brand = uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_read_u_read)) >> 8,
    },
    {  // RAM
      .memory_index = kabi::well_known_object_count + 1,
      .brand = uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_write_u_write)
          .with_xn(true)) >> 8,
    },
  },

  .memory_map = {},
};

static constexpr size_t object_table_count =
  kabi::well_known_object_count
  + config::memory_map_count
  + config::device_map_count
  + config::extra_slot_count;

__attribute__((section(".donated_ram")))
uint8_t kernel_donation[
  kabi::context_size  // first context
  + kabi::object_head_size * object_table_count  // Object Table
  + sizeof(void *) * (1 + config::external_interrupt_count)];

static constexpr unsigned
  oi_sys_rom = kabi::well_known_object_count + 0,
  oi_app_ram = kabi::well_known_object_count + 2,
  oi_apb = kabi::well_known_object_count + 3;


/*******************************************************************************
 * Factored out bits.
 */

/*
 * Allocates a fresh Gate.
 */
static rt::AutoKey make_gate() {
  auto k = etl::move(alloc_mem(kabi::gate_l2_size - 1, 0).ref());
  memory::become(k, memory::ObjectType::gate, 0);
  return k;
}


/*******************************************************************************
 * Startup steps.
 */

static void feed_allocator() {
  // Mint a key to the application RAM object described above.
  auto k_app_ram = object_table::mint_key(ki::ot, oi_app_ram, 0);
  // Hand said object over to the allocator.
  bool freed = free_mem(k_app_ram);
  // If the allocator doesn't take it, we have a bug!
  ETL_ASSERT(freed);
}

static void make_self_key() {
  // Get a context key to ourselves.
  auto k_self = object_table::mint_key(ki::ot, kabi::oi_first_context, 0);
  // Put it in the appointed place.
  rt::copy_key(ki::self, k_self);
}

static void make_idle_task() {
  // Allocate space for a Context.
  auto k_ctx = etl::move(alloc_mem(kabi::context_l2_size - 1, 0).ref());

  // Make it into a real Context.
  memory::become(k_ctx, memory::ObjectType::context, 0);

  prepare_idle_task(k_ctx);

  // Make it runnable so we can block.
  context::make_runnable(k_ctx);
}

static void make_syscall_gate() {
  auto k = make_gate();
  rt::copy_key(ki::syscall_gate, k);
}


/*******************************************************************************
 * Syscall server.
 */

static constexpr auto base_receive_descriptor = Descriptor::zero()
  .with_receive_enabled(true)
  .with_source(ki::syscall_gate);

static void make_zero_reply(unsigned k, Message & msg) {
  msg = {
    base_receive_descriptor
      .with_send_enabled(true)
      .with_target(k),
  };
}

static void make_error_reply(unsigned k, Message & msg, uint64_t exc) {
  msg = {
    base_receive_descriptor
      .with_send_enabled(true)
      .with_target(k)
      .with_error(true),
    uint32_t(exc),
    uint32_t(exc >> 32),
  };
}

static void make_error_reply(unsigned k, Message & msg, Exception exc) {
  make_error_reply(k, msg, uint64_t(exc));
}

__attribute__((noreturn))
static void serve_syscalls() {
  // Storage for message exchange and brand.
  Message msg {
    base_receive_descriptor,
  };
  uint64_t brand;

  // Reserve key registers for keys sent by clients.
  auto k_client_reply = rt::AutoKey{};
  auto k_tmp1 = rt::AutoKey{};

  // The request_map determines which keys we *accept* from clients, and it's
  // always the same.
  auto request_map = rt::keymap(k_client_reply, k_tmp1);

  // We'll rewrite the reply_map each round, to determine which keys we send
  // in the reply.
  auto reply_map = rt::keymap();

  while (true) {
    rt::ipc2(msg, reply_map, request_map, &brand);
    // Reset the reply_map to avoid sending authority by accident.
    reply_map = rt::keymap();

    // Reject any errors delivered this way.
    if (msg.desc.get_error()) {
      // Don't bother sending a reply.
      msg.desc = base_receive_descriptor;
      continue;
    }

    // TODO: eventually, we'll need to behave differently based on brand, e.g.
    // for interrupts.

    // The OTI of the caller's context is given by the low 16 bits of the
    // brand.   Extract it.
    auto caller_oti = uint32_t(brand) & 0xFFFF;

    switch (msg.desc.get_selector()) {
      case selector::map_memory:
        {
          // Mint a powerful service key to the caller's Context.
          auto k_ctx = object_table::mint_key(ki::ot, caller_oti, 0);
          // Install the requested key.
          context::set_region(k_ctx, msg.d0, k_tmp1);
          // Prepare a reply.
          make_zero_reply(k_client_reply, msg);
        }
        break;

      case selector::exit:
        {
          // Mint a powerful service key to the caller's Context.
          auto k_ctx = object_table::mint_key(ki::ot, caller_oti, 0);
          // Effectively halt the system.  This is obviously wrong (TODO)
          while (true);
        }
        break;

      default:
        make_error_reply(k_client_reply, msg, Exception::bad_operation);
        break;
    }
  }
}

/*******************************************************************************
 * Actual startup.
 */

__attribute__((noreturn))
static void main() {
  rt::reserve_key(ki::ot);
  rt::reserve_key(ki::self);
  rt::reserve_key(ki::syscall_gate);

  feed_allocator();
  make_self_key();
  make_syscall_gate();
  make_idle_task();

  ::sys::app_setup();

  serve_syscalls();
}

}  // namespace sketch
}  // namespace sys

// Entry point expected by ETL CRT0
int main() {
  sys::sketch::main();
}
