#include "demo/driver.h"

#include <cstdint>

#include "etl/stm32f4xx/gpio.h"
#include "etl/stm32f4xx/rcc.h"
#include "etl/stm32f4xx/usart.h"

#include "demo/ipc.h"

using etl::stm32f4xx::Gpio;
using etl::stm32f4xx::gpioa;
using etl::stm32f4xx::rcc;
using etl::stm32f4xx::Usart;
using etl::stm32f4xx::usart2;

namespace demo {

static constexpr unsigned apb1_hz = 16000000, console_baud = 115200;

// Our use of key registers:
static constexpr unsigned
  k_gate = 4;

volatile uint32_t receive_count;
volatile uint32_t send_count;

void driver_main() {
  receive_count = 0;
  send_count = 0;

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
      .with_block(true)
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
