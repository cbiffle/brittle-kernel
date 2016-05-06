#include <cstdint>

#include "etl/assert.h"
#include "etl/armv7m/exception_frame.h"
#include "etl/armv7m/instructions.h"
#include "etl/armv7m/mpu.h"
#include "etl/data/range_ptr.h"
#include "etl/stm32f4xx/interrupts.h"
#include "etl/armv7m/implicit_crt0.h"
#include "etl/armv7m/exception_table.h"

#include "common/app_info.h"
#include "common/abi_sizes.h"

#include "demo/ascii/client.h"
#include "demo/drv/uart/driver.h"
#include "demo/k/context.h"
#include "demo/k/interrupt.h"
#include "demo/k/memory.h"
#include "demo/k/object_table.h"
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
  extern uint32_t _app_ram1_start, _app_ram1_end;
}

__attribute__((section(".app_info0")))
__attribute__((used))
constexpr AppInfo app_info {
  .abi_token = current_abi_token,

  .memory_map_count = 4,
  .extra_slot_count = 16,
  .external_interrupt_count = uint32_t(etl::stm32f4xx::Interrupt::usart2) + 1,

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
  {
    // 6: Memory describing the peripheral region.
    0x40000000,
    0x60000000,
  },
  {
    // 7: Memory describing top half of app RAM
    reinterpret_cast<uint32_t>(&_app_ram1_start),
    reinterpret_cast<uint32_t>(&_app_ram1_end),
  },
};

__attribute__((section(".donated_ram")))
uint8_t kernel_donation[2048];

// Our use of key registers:
static constexpr unsigned
  k_object_table = 4,
  k_periph = 5,
  k_self = 6,
  k_second = 7,
  k_idle = 8,
  k_gate = 9,
  k_irq_gate = 10,
  k_irq = 11,
  k_tmp0 = 12,
  k_tmp1 = 13;


/*******************************************************************************
 * Slot allocator.
 */

static constexpr unsigned fixed_slot_count =
  4 + app_info.memory_map_count;

static uint8_t slot_used[app_info.extra_slot_count];
static unsigned slot_use_count;

static unsigned alloc_slot() {
  for (unsigned i = 0; i < app_info.extra_slot_count; ++i) {
    if (!slot_used[i]) {
      slot_used[i] = 1;
      slot_use_count++;
      return i + fixed_slot_count;
    }
  }

  // Don't tolerate allocation failures.
  ETL_ASSERT(false);
}


/*******************************************************************************
 * Memory allocator.
 */

static unsigned mem_roots[32];

static void init_mem() {
  mem_roots[15] = 7;
  object_table::mint_key(k_object_table, 7, 0, k_tmp1);
  memory::poke(k_tmp1, 0, 0);
}

static unsigned alloc_mem(unsigned l2_half_size, unsigned key_out) {
  ETL_ASSERT(l2_half_size < 32);
  ETL_ASSERT(l2_half_size > 3);

  unsigned index = mem_roots[l2_half_size];

  // If we have the index of a suitably-sized memory...
  if (index) {
    // ...then mint a key to it...
    ETL_ASSERT(object_table::mint_key(k_object_table, index, 0, key_out));
    // ...and use that to load the link index, freeing it.
    mem_roots[l2_half_size] = memory::peek(key_out, 0);
    return index;
  } else {
    // We're going to have to split some memory.  Allocate a chunk twice as
    // large as we need.
    unsigned bottom_index = alloc_mem(l2_half_size + 1, key_out);
    // Allocate an object table index to hold its top half.
    unsigned top_index = alloc_slot();
    // Fabricate a slot key.
    ETL_ASSERT(object_table::mint_key(k_object_table, top_index, 0, k_tmp1));
    // Perform the split.
    memory::split(key_out, k_tmp1, k_tmp1);
    // Link the new top into place as the only member of the freelist for this
    // size.
    memory::poke(k_tmp1, 0, 0);  // End of list.
    mem_roots[l2_half_size] = top_index;
    // We now have a suitably-sized memory key in key_out.
    return bottom_index;
  }
}


/*******************************************************************************
 * Context maintenance utilities.
 */

/*
 * Duplicates one Context's MPU configuration into another.
 */
static void copy_all_regions(unsigned from_k, unsigned to_k) {
  for (unsigned i = 0; i < 6; ++i) {
    context::get_region(from_k, i, 1);
    context::set_region(to_k, i, 1);
  }
}

/*
 * Sets up the machine registers, including the stack pointer, of a Context.
 */
static void initialize_registers(unsigned k,
                                 RangePtr<uint32_t> stack,
                                 void (*entry)()) {
  using etl::armv7m::ExceptionFrame;

  // Fill in the initial register values on the stack.
  auto frame = reinterpret_cast<ExceptionFrame *>(&stack[stack.count() - 8]);
  frame->r14 = 0xFFFFFFFF;  // catch unintentional exit from entry point
  frame->r15 = reinterpret_cast<uint32_t>(entry);
  frame->psr = 1 << 24;  // set thumb mode

  // Load the stack pointer.
  context::set_register(k, context::Register::sp,
      reinterpret_cast<uint32_t>(frame));
}


/*******************************************************************************
 * Idle task implementation.  This didn't seem to warrant its own file.
 */

static uint32_t idle_stack[128];

static void idle_main() {
  while (true) {
    etl::armv7m::wait_for_interrupt();
  }
}


/*******************************************************************************
 * Demo environment.
 */

// Some object indices of which we're aware:
static constexpr unsigned
  oi_initial_context = 2,
  oi_rom_region = 4,
  oi_ram_region = 5,
  oi_peripheral_region = 6,
  oi_extra_ram = 7;

static void derive_initial_authority() {
  // Mint a key to our own context.  We need this, at first, to read out the
  // initial memory keys generated by the kernel.
  object_table::mint_key(k_object_table, oi_initial_context, 0, k_self);
}

static void init_irq() {
  // Create the interrupt sender.
  alloc_mem(kabi::interrupt_l2_size - 1, k_irq);
  memory::become(k_irq,
                 memory::ObjectType::interrupt,
                 uint32_t(etl::stm32f4xx::Interrupt::usart2));

  // Create the interrupt gate.
  alloc_mem(kabi::gate_l2_size - 1, k_irq_gate);
  memory::become(k_irq_gate, memory::ObjectType::gate, 0);

  // Wire the IRQ sender to fire at the IRQ gate.
  interrupt::set_target(k_irq, k_irq_gate);
}

static void init_ipc() {
  // Create the IPC gate.
  alloc_mem(kabi::gate_l2_size - 1, k_gate);
  memory::become(k_gate, memory::ObjectType::gate, 0);
}

static void spawn_idle() {
  // Create a Context to describe the idle task.
  alloc_mem(kabi::context_l2_size - 1, k_idle);
  memory::become(k_idle, memory::ObjectType::context, 0);

  // Load its memory region keys so it can actually run.  For now, we'll just
  // share our memory authority.  TODO: it could be limited further.
  copy_all_regions(k_self, k_idle);

  // Fill in its registers and stack.
  initialize_registers(k_idle, idle_stack, idle_main);

  // Provide it with the gate key, so it can do something.
  context::set_key(k_idle, 4, k_gate);

  // Set up its execution parameters and let it run.
  // It won't run yet, of course; we haven't yielded control.
  context::set_priority(k_idle, 1);
  context::make_runnable(k_idle);
}

static uint32_t client_stack[128];

static void spawn_client() {
  // Create a Context for the client task.
  alloc_mem(kabi::context_l2_size - 1, k_second);
  memory::become(k_second, memory::ObjectType::context, 0);

  // Load its memory region keys so it can actually run.  For now, we'll just
  // share our memory authority.  TODO: it could be limited further.
  copy_all_regions(k_self, k_second);

  // Fill in its registers and stack.
  initialize_registers(k_second, client_stack, client_main);

  // Provide it with the gate key, so it can do something.
  context::set_key(k_second, 4, k_gate);

  // Set up its execution parameters and let it run.
  // It won't run yet, of course; we haven't yielded control.
  context::make_runnable(k_second);
}

__attribute__((noreturn))
static void demo_main() {
  // Exercise our authority to create objects needed for the demo.
  derive_initial_authority();
  init_mem();
  init_irq();
  init_ipc();

  // Fire up the other tasks.
  spawn_idle();
  spawn_client();

  // Now, become a UART driver.  Grant access to the APB.
  object_table::mint_key(k_object_table,
                         oi_peripheral_region,
                         0x13000037 >> 8,
                         k_periph);
  context::set_region(k_self, 2, k_periph);

  // Arrange keys in the way the driver expects and discard extra authority.
  copy_key(4, k_gate);
  copy_key(5, k_irq_gate);
  copy_key(6, k_irq);

  for (unsigned i = 0; i < 4; ++i) copy_key(i, 0);
  for (unsigned i = 7; i < 16; ++i) copy_key(i, 0);

  drv::uart::main();
}

}  // namespace demo

int main() {
  demo::demo_main();
}
