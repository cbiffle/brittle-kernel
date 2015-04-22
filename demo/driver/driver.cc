#include "demo/driver/driver.h"

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

// Our use of key registers:
static constexpr unsigned
  k_gate = 4,
  k_irq_gate = 5,
  k_irq = 6,
  k_saved_reply = 7;

volatile uint32_t receive_count;
volatile uint32_t send_count;

static void driver_setup() {
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

static void wait_for_txe() {
  auto rm = ipc({
      Descriptor::zero()
        .with_receive_enabled(true)
        .with_source(k_irq_gate)
        .with_block(true)});
  ETL_ASSERT(rm.m.d0.get_error() == false);

  usart2.write_cr1(usart2.read_cr1().with_txeie(false));

  // TODO treat as call?
  interrupt::enable(k_irq, true);
}

void driver_main() {
  receive_count = 0;
  send_count = 0;

  driver_setup();

  while (true) {
    auto rm = ipc({
      Descriptor::zero()
        .with_receive_enabled(true)
        .with_source(k_gate)
        .with_block(true)
      });
    ++receive_count;

    if (rm.m.d0.get_error()) {
      // Don't bother replying.
      continue;
    }

    switch (rm.m.d0.get_selector()) {
      case 1:  // Send!
        ++send_count;
        // Back up reply key, as we're about to receive from the IRQ.
        copy_key(k_saved_reply, 0);
        // Load character into data register.
        usart2.write_dr(rm.m.d1 & 0xFF);
        // Enable TX Empty interrupt (signals data register available)
        usart2.write_cr1(usart2.read_cr1().with_txeie(true));

        wait_for_txe();

        ipc({
            Descriptor::zero()
              .with_send_enabled(true)
              .with_target(k_saved_reply)
              });
        continue;

      default:  // Crap!
        ipc({
            Descriptor::zero()
              .with_send_enabled(true)
              .with_target(0)
              .with_error(true)
              });
        // Technically not a valid exception spec (TODO)
        continue;
    }
  }
}

}  // namespace demo
