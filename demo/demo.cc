#include <cstdint>

#include "demo/client.h"
#include "demo/context.h"
#include "demo/driver.h"
#include "demo/ipc.h"
#include "demo/object_table.h"

namespace demo {

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

void main() {
  // Derive our authority from the kernel's representation.
  collect_keys();
  // Fire up the client.
  spawn_client();

  // Now, become a UART driver.  Grant access to the APB.
  enable_peripheral_access();
  // Arrange keys in the way the driver expects and discard extra authority.
  copy_key(4, k_gate);
  for (unsigned i = 5; i < 16; ++i) {
    copy_key(i, 0);
  }

  driver_main();
}

}  // namespace demo
