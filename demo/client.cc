#include "demo/client.h"

#include <cstdint>

#include "demo/ipc.h"

namespace demo {

uint32_t volatile client_ipc_issue_count;
uint32_t volatile client_ipc_complete_count;
uint32_t volatile client_error_count;

void client_main() {
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

}  // namespace demo
