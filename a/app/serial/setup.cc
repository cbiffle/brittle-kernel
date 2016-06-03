#include "etl/armv7m/mpu.h"
#include "etl/stm32f4xx/interrupts.h"

#include "common/abi_sizes.h"
#include "common/app_info.h"

#include "a/rt/keys.h"

#include "a/k/context.h"
#include "a/k/gate.h"
#include "a/k/interrupt.h"
#include "a/k/memory.h"
#include "a/k/object_table.h"

#include "a/sys/alloc.h"
#include "a/sys/keys.h"
#include "a/sys/load.h"
#include "a/sys/setup.h"

#include "peanut_config.h"

using etl::armv7m::Mpu;
using Rasr = Mpu::rasr_value_t;

static_assert(
    config::external_interrupt_count
        > uint32_t(etl::stm32f4xx::Interrupt::usart2),
    "Not enough interrupts enabled in peanut_config.h");


extern "C" {
  extern uint32_t _donated_ram_begin, _donated_ram_end;
  extern uint32_t _sys_initial_stack;

  extern uint32_t _sys_rom_start, _sys_rom_end;
  extern uint32_t _sys_ram0_start, _sys_ram0_end;
  extern uint32_t _app_ram1_start, _app_ram1_end;
}

namespace sys {

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
  {
    // 7: APB
    0x40000000,
    0x60000000,
  },
};

static constexpr unsigned
  oi_object_table = 1,
  oi_first_ctx = 2,
  oi_sys_rom = 4,
  oi_app_ram = 6,
  oi_apb = 7;


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
 * Placeholder for the app initialization sequence.
 */

extern "C" {
  extern uint32_t const _prog_drv_stm32f4_uart;
  extern uint32_t const _prog_srv_ascii;
}

static rt::AutoKey make_uart_driver() {
  auto addr = reinterpret_cast<uintptr_t>(&_prog_drv_stm32f4_uart);

  // Make an isolated image key.
  auto k_img = [addr]{
      auto k_rom = object_table::mint_key(ki::ot, oi_sys_rom,
        uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_read_u_read)) >> 8);
      auto k_slot = alloc_slot();
      memory::make_child(k_rom, addr, 2048, k_slot);
      return k_slot;
      }();
  
  // Give it to the program loader.
  auto maybe_k_prog = load_program(addr, k_img);
  auto & k_prog = maybe_k_prog.ref();
  
  // Make a specialized syscall key using the program's OT index as brand.
  {
    auto prog_info = object_table::read_key(ki::ot, k_prog);
    auto k_sys = gate::make_client_key(ki::syscall_gate,
        (Brand(1) << 63) | prog_info.index);
    context::set_key(k_prog, 15, k_sys);
  }

  // Make it a gate for client requests.  Keep it around to return it.
  auto k_gate = make_gate();
  context::set_key(k_prog, 14, k_gate);

  // Make an actual Interrupt for the driver to use, and a gate to use with it.
  // TODO: totally hardcoded vector number here.
  // TODO: it's silly that we set the Interrupt target; driver should do it
  // so that it can choose the brand.
  {
    auto k_irq = etl::move(alloc_mem(kabi::interrupt_l2_size - 1, 0).ref());
    memory::become(k_irq, memory::ObjectType::interrupt,
        uint32_t(etl::stm32f4xx::Interrupt::usart2));
    context::set_key(k_prog, 12, k_irq);

    auto k_irq_gate = make_gate();
    context::set_key(k_prog, 13, k_irq_gate);

    auto k_irq_gate_client = gate::make_client_key(k_irq_gate, Brand(1) << 63);
    interrupt::set_target(k_irq, k_irq_gate_client);
  }

  // Give the driver access to the APB.
  {
    auto k_apb = object_table::mint_key(ki::ot, oi_apb,
        uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_write_u_write)
          .with_xn(true)) >> 8);
    context::set_key(k_prog, 11, k_apb);
  }

  context::set_priority(k_prog, 0);
  context::make_runnable(k_prog);

  return k_gate;
}

static void make_uart_client(unsigned k_uart_gate) {
  auto addr = reinterpret_cast<uintptr_t>(&_prog_srv_ascii);

  // Make an isolated image key.
  auto k_img = [addr]{
      auto k_rom = object_table::mint_key(ki::ot, oi_sys_rom,
        uint32_t(Rasr()
          .with_ap(Mpu::AccessPermissions::p_read_u_read)) >> 8);
      auto k_slot = alloc_slot();
      memory::make_child(k_rom, addr, 512, k_slot);
      return k_slot;
      }();
 
  // Give it to the program loader.
  auto maybe_k_prog = load_program(addr, k_img);
  auto & k_prog = maybe_k_prog.ref();
  
  // Make a specialized syscall key using the program's OT index as brand.
  {
    auto prog_info = object_table::read_key(ki::ot, k_prog);
    auto k_sys = gate::make_client_key(ki::syscall_gate,
        (Brand(1) << 63) | prog_info.index);
    context::set_key(k_prog, 15, k_sys);
  }

  // Give it the UART gate.
  {
    auto k_uart_client = gate::make_client_key(k_uart_gate, Brand(1) << 63);
    context::set_key(k_prog, 14, k_uart_client);
  }

  context::set_priority(k_prog, 0);
  context::make_runnable(k_prog);
}

void app_setup() {
  auto k_uart_gate = make_uart_driver();
  make_uart_client(k_uart_gate);
}

}  // namespace sys
