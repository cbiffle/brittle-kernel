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

#include "demo/runtime/ipc.h"

namespace demo {

// Symbolic names for our key registers.
static constexpr unsigned k_uart = 4;

// Mnemonic name for the single message selector we use in the UART protocol.
static constexpr unsigned s_send = 1;

/*
 * Diagnostic variables, intended to be viewed through GDB.
 */
uint32_t volatile client_ipc_issue_count;
uint32_t volatile client_ipc_complete_count;
uint32_t volatile client_error_count;

void client_main() {
  // We're not entirely sure that the CRT will zero our BSS in this demo
  // environment, so let's be pedantic:
  client_ipc_issue_count = 0;
  client_ipc_complete_count = 0;
  client_error_count = 0;

  while (true) {
    for (uint8_t value = ' '; value < 127; ++value) {
      // Prepare the message.
      Message m {
        Descriptor::call(s_send, k_uart),
        value,
      };

      // Send it.
      ++client_ipc_issue_count;
      auto rm = ipc(m);
      ++client_ipc_complete_count;

      // Record errors that occur.
      if (rm.m.d0.get_error()) ++client_error_count;
    }
  }
}

}  // namespace demo
