/*
 * A UART driver for the STM32F4.  Send-only for now.
 *
 * Startup environment:
 * - KR15: system gate
 * - KR14: UART client gate
 * - KR13: UART interrupt gate
 * - KR12: interrupt
 * - KR11: APB memory window
 *
 * Protocol:
 * - Selector 1: send character from low 8 bits of D0.
 */

#include <cstdint>

#include "etl/assert.h"
#include "etl/stm32f4xx/gpio.h"
#include "etl/stm32f4xx/rcc.h"
#include "etl/stm32f4xx/usart.h"

#include "a/k/interrupt.h"
#include "a/rt/crt0.h"
#include "a/rt/ipc.h"
#include "a/rt/keys.h"
#include "a/sys/api.h"

using etl::stm32f4xx::Usart;
using etl::stm32f4xx::usart2;

// Our use of key registers:
static constexpr unsigned
  k_discard = 0,
  k_sender_reply = 1,
  k_irq_reply = 2,
  k_reg_mem = 11,
  k_irq = 12,
  k_irq_gate = 13,
  k_gate = 14,
  k_sys = 15;

// Diagnostic variables for GDB.
volatile uint32_t receive_count;
volatile uint32_t send_count;

/*
 * Initialize hardware and whatnot.
 */
static void initialize_hardware() {
  // Make the APB directly accessible.
  sys::map_memory(k_sys, k_reg_mem, 2);

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
  return rt::ipc({
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
      rt::keymap(k_irq_reply, k_discard, k_discard, k_discard));
  // The IRQ object should not be sending errors!
  ETL_ASSERT(rm.m.desc.get_error() == false);

  // Disable TXE interrupt generation; otherwise it'll happen repeatedly.
  usart2.write_cr1(usart2.read_cr1().with_txeie(false));

  // Re-enable interrupts.
  interrupt::enable(k_irq_reply, true);
}

/*
 * Main driver loop.
 */
__attribute__((noreturn))
int main() {
  rt::reserve_key(k_sender_reply);
  rt::reserve_key(k_irq_reply);
  rt::reserve_key(k_reg_mem);
  rt::reserve_key(k_irq);
  rt::reserve_key(k_irq_gate);
  rt::reserve_key(k_gate);
  rt::reserve_key(k_sys);

  initialize_hardware();

  while (true) {
    auto rm = blocking_receive(k_gate,
        rt::keymap(k_sender_reply, k_discard, k_discard, k_discard));
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

        rt::ipc({
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
        rt::ipc({
            Descriptor::zero()
              .with_send_enabled(true)
              .with_target(0)
              .with_error(true)
              }, 0, 0);
        continue;
    }
  }
}
