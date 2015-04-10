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
  oi_initial_context = 5;

// Our use of key registers:
static constexpr unsigned
  k_object_table = 4,
  k_periph = 5,
  k_self = 6;

void main() {
  // Awkwardly produce a key to the peripheral address space region.
  object_table::mint_key(k_object_table,
                         oi_peripheral_region,
                         0x1300003740000000ULL,
                         k_periph);
  // Slightly less awkwardly produce our own context key.
  object_table::mint_key(k_object_table, oi_initial_context, 0, k_self);
  // Enable access to peripherals, using region 2.
  context::set_region(k_self, 2, k_periph);

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

  counter = 0;
  while (true) {
    while (usart2.read_sr().get_txe() == false);
    usart2.write_dr(counter & 0xFF);
    ++counter;
  }
}

}  // namespace demo
