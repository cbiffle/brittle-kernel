/*
 * A UART driver for the STM32F4, implemented as an RPC server.  Send-only for
 * now.
 *
 * Expects:
 * - Input message gate in KR4.
 * - Gate from which to receive interrupts in KR5.
 * - Interrupt object key in KR6.
 * 
 * Protocol:
 * - Selector 1: send character from low 8 bits of D1.
 */

#include "demo/drv/uart/driver.h"

#include <cstdint>

#include "etl/assert.h"
#include "etl/stm32f4xx/gpio.h"
#include "etl/stm32f4xx/rcc.h"
#include "etl/stm32f4xx/usart.h"

#include "demo/k/interrupt.h"
#include "demo/runtime/ipc.h"

using etl::stm32f4xx::Usart;
using etl::stm32f4xx::usart2;

namespace demo {
namespace drv {
namespace uart {

// Our use of key registers:
static constexpr unsigned
  k_discard = 0,
  k_gate = 4,
  k_irq_gate = 5,
  k_irq = 6,
  k_sender_reply = 7,
  k_irq_reply = 8;

// Diagnostic variables for GDB.
volatile uint32_t receive_count;
volatile uint32_t send_count;

/*
 * Initialize hardware and whatnot.
 */
static void initialize_hardware() {
  using etl::stm32f4xx::Gpio;
  using etl::stm32f4xx::gpioa;
  using etl::stm32f4xx::rcc;

  static constexpr unsigned apb1_hz = 16000000, console_baud = 115200;

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

  // Enable IRQ.
  interrupt::enable(k_irq);
}

// Utility function for blocking receive.
static ReceivedMessage blocking_receive(unsigned k, uint32_t recv_map) {
  return ipc({
      Descriptor::zero()
      .with_receive_enabled(true)
      .with_source(k)
      .with_block(true)},
      0,
      recv_map);
}

/*
 * Blocks waiting for a TXE interrupt from the UART, disables further TXE
 * interrupts, and leaves the general UART interrupt enabled.
 */
static void wait_for_txe() {
  // Receive from the IRQ gate, blocking.
  auto rm = blocking_receive(k_irq_gate,
      keymap(k_irq_reply, k_discard, k_discard, k_discard));
  // The IRQ object should not be sending errors!
  ETL_ASSERT(rm.m.desc.get_error() == false);

  // Disable TXE interrupt generation; otherwise it'll happen repeatedly.
  usart2.write_cr1(usart2.read_cr1().with_txeie(false));

  // Re-enable interrupts.
  auto r = interrupt::enable(k_irq_reply, true);
  ETL_ASSERT(r);
}

/*
 * Main driver loop.
 */
void main() {
  initialize_hardware();

  while (true) {
    auto rm = blocking_receive(k_gate,
        keymap(k_sender_reply, k_discard, k_discard, k_discard));
    ++receive_count;

    if (rm.m.desc.get_error()) {
      // Clients can send us errors if they want.  Don't bother replying.
      // If the client sent an error in a "call" this may block the client
      // forever... not our problem.  Better this than to reply blindly and
      // potentially cause an error storm.
      continue;
    }

    switch (rm.m.desc.get_selector()) {
      case 1:  // Send!
        ++send_count;
        // Load character into data register.
        usart2.write_dr(rm.m.d0 & 0xFF);
        // Enable TX Empty interrupt (signals data register available)
        usart2.write_cr1(usart2.read_cr1().with_txeie(true));

        wait_for_txe();

        ipc({
            Descriptor::zero()
              .with_send_enabled(true)
              .with_target(k_sender_reply)
              }, 0, 0);
        continue;

      default:  // Bogus message selector.
        // This may be a genuine mistake, so we send an error back without
        // blocking.
        // TODO: technically this is not a valid Exception in the new error
        // model.
        ipc({
            Descriptor::zero()
              .with_send_enabled(true)
              .with_target(0)
              .with_error(true)
              }, 0, 0);
        continue;
    }
  }
}

}  // namespace uart
}  // namespace drv
}  // namespace demo
