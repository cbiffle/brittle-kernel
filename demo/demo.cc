#include <cstdint>

#include "demo/ipc.h"
#include "demo/object_table.h"
#include "demo/context.h"

#include "etl/stm32f4xx/gpio.h"
#include "etl/stm32f4xx/rcc.h"
#include "etl/stm32f4xx/usart.h"

using etl::stm32f4xx::Gpio;
using etl::stm32f4xx::gpioa;
using etl::stm32f4xx::rcc;
using etl::stm32f4xx::Usart;
using etl::stm32f4xx::usart2;

namespace demo {

static constexpr unsigned apb1_hz = 16000000, console_baud = 115200;

static unsigned volatile counter;

void main();

// Some object indices of which we're aware:
static constexpr unsigned
  oi_peripheral_region = 4,
  oi_gate = 5,
  oi_initial_context = 6,
  oi_second_context = 7;

// Our use of key registers:
static constexpr unsigned
  k_object_table = 4,
  k_periph = 5,
  k_self = 6,
  k_second = 7,
  k_gate = 8;

static void collect_keys() {
  // Awkwardly produce a key to the peripheral address space region.
  object_table::mint_key(k_object_table,
                         oi_peripheral_region,
                         0x1300003740000000ULL,
                         k_periph);
  // Slightly less awkwardly produce our own context key.
  object_table::mint_key(k_object_table, oi_initial_context, 0, k_self);
  // Get a key to the second context.
  object_table::mint_key(k_object_table, oi_second_context, 0, k_second);
  // aaaand the IPC gate.
  object_table::mint_key(k_object_table, oi_gate, 0, k_gate);
}

static void enable_peripheral_access() {
  context::set_region(k_self, 2, k_periph);
}

uint32_t volatile client_ipc_issue_count;
uint32_t volatile client_ipc_complete_count;
uint32_t volatile client_error_count;

static void client_main() {
  client_ipc_issue_count = 0;
  client_ipc_complete_count = 0;
  client_error_count = 0;

  uint8_t value = 0;
  while (true) {
    Message m {
      Descriptor::call(1, 4),
      value,
    };
    ++value;
    ++client_ipc_issue_count;
    auto rm = ipc(m);
    ++client_ipc_complete_count;

    if (rm.m.d0.get_error()) ++client_error_count;
  }
}

static uint32_t client_stack[128];

static void spawn_client() {
  // Inflate the nascent Context into a little task.

  // Load its memory region keys so it can actually run.  For now, we'll just
  // share our memory authority.  TODO: it could be limited further.
  for (unsigned i = 0; i < 6; ++i) {
    context::get_region(k_self, i, 1);
    context::set_region(k_second, i, 1);
  }

  // Give it a valid stack.  We need to do this before setting machine
  // registers, as some of them live in the exception frame at the moment.
  auto second_stack_frame = &client_stack[128 - 8];
  context::set_register(k_second, 13,
      reinterpret_cast<uint32_t>(second_stack_frame));

  // Now, set the machine registers.
  for (unsigned i = 0; i < 17; ++i) {
    uint32_t register_value;
    switch (i) {
      case 13:
        continue;  // handled above.

      case 15:
        register_value = reinterpret_cast<uint32_t>(client_main);
        break;

      case 16:
        register_value = 1 << 24;  // set thumb mode
        break;

      default:
        register_value = 0xDEADBEEF;
        break;
    }

    context::set_register(k_second, i, register_value);
  }

  // Provide it with the gate key, so it can do something.
  context::set_key(k_second, 4, k_gate);

  // Set up its execution parameters and let it run.
  // It won't run yet, of course; we haven't yielded control.
  context::make_runnable(k_second);
}

volatile uint32_t receive_count;
volatile uint32_t send_count;

void main() {
  receive_count = 0;
  send_count = 0;

  collect_keys();
  spawn_client();

  // Now, become a UART driver.
  enable_peripheral_access();

  // Enable clocking to the UART.
  rcc.write_apb1enr(rcc.read_apb1enr()
      .with_bit(17, true));
  // Enable clock to GPIOA.
  rcc.write_ahb1enr(rcc.read_ahb1enr()
      .with_bit(0, true));

  // Configure USART2.
  usart2.write_cr1(Usart::cr1_value_t()
      .with_ue(true));
  usart2.write_brr(apb1_hz / console_baud);
  usart2.write_cr2(usart2.read_cr2()
      .with_stop(Usart::cr2_value_t::stop_t::two));
  usart2.write_cr1(usart2.read_cr1()
      .with_te(true)
      .with_re(true));

  // Configure GPIOs.
  gpioa.set_mode(Gpio::p2 | Gpio::p3, Gpio::Mode::alternate);
  gpioa.set_alternate_function(Gpio::p2 | Gpio::p3, 7);

  Message m {
    Descriptor::zero()
      .with_receive_enabled(true)
      .with_source(k_gate)
  };
  while (true) {
    auto rm = ipc(m);
    ++receive_count;

    if (rm.m.d0.get_error()) {
      // Don't bother replying.
      m.d0 = m.d0.with_send_enabled(false);
      continue;
    }

    switch (rm.m.d0.get_selector()) {
      case 1:  // Send!
        ++send_count;
        while (usart2.read_sr().get_txe() == false);
        usart2.write_dr(rm.m.d1 & 0xFF);

        m.d0 = m.d0.with_send_enabled(true)
                   .with_target(0)
                   .with_error(false);
        m.d1 = m.d2 = m.d3 = 0;
        break;

      default:  // Crap!
        m.d0 = m.d0.with_send_enabled(true)
                   .with_target(0)
                   .with_error(true);
        // Technically not a valid exception spec (TODO)
        m.d1 = m.d2 = m.d3 = 0;
    }
  }
}

}  // namespace demo
