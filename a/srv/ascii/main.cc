/*
 * A largely useless server which blows messages at a gate.  The first data
 * word in the message counts through the printable ASCII character set.  This
 * is designed to drive a UART.
 *
 * Startup environment:
 * - KR15: system gate
 * - KR14: UART client gate
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
  k_gate = 14,
  k_sys = 15;

__attribute__((noreturn))
int main() {
  rt::reserve_key(k_gate);
  rt::reserve_key(k_sys);

  while (true) {
    for (unsigned c = ' '; c < 127; ++c) {
      Message m {
        Descriptor::call(1, k_gate),
        c,
      };
      rt::ipc2(m, 0, 0);
      ETL_ASSERT(!m.desc.get_error());
    }
  }
}
