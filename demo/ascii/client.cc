/*
 * Demonstration client for the UART driver.  Uses RPC to repeatedly transmit a
 * printable subset of 7-bit ASCII.
 *
 * Expects:
 * - A gate key for reaching the UART in KR4.
 * - Enough memory authority to run (does not need hardware access).
 */

#include "demo/client/client.h"

#include <cstdint>

#include "demo/driver/api.h"

namespace demo {

// Symbolic names for our key registers.
static constexpr unsigned k_uart = 4;

/*
 * Diagnostic variables, intended to be viewed through GDB.
 */
uint32_t volatile client_ipc_issue_count;
uint32_t volatile client_ipc_complete_count;
uint32_t volatile client_error_count;

void client_main() {
  while (true) {
    for (uint8_t value = ' '; value < 127; ++value) {
      ++client_ipc_issue_count;
      bool success = uart::send(k_uart, value);
      ++client_ipc_complete_count;

      // Record errors that occur.
      if (!success) ++client_error_count;
    }
  }
}

}  // namespace demo
