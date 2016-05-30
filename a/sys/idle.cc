#include "a/sys/idle.h"

#include "etl/array_count.h"

#include "a/k/context.h"
#include "a/sys/keys.h"

namespace sys {

static uint32_t idle_stack[16];

static void idle_main() {
  while (true);
}

void prepare_idle_task(unsigned k_ctx) {
  // Give it our authority over address space.
  for (unsigned i = 0; i < 2; ++i) {
    auto k_reg = context::get_region(ki::self, i);
    context::set_region(k_ctx, i, k_reg);
  }

  // Prepare a stack frame.
  // PSR on top
  idle_stack[etl::array_count(idle_stack) - 1] = 1 << 24;
  // Then PC
  idle_stack[etl::array_count(idle_stack) - 2]
    = reinterpret_cast<uint32_t>(idle_main);
  // Rest doesn't matter.
  // Load it into SP.
  context::set_register(k_ctx, context::Register::sp,
      reinterpret_cast<uint32_t>(
        &idle_stack[etl::array_count(idle_stack) - 8]));

  // Set the task to a slightly lower priority.
  // TODO: should be the lowest priority, but there's no easy way to discover
  // this right now.
  context::set_priority(k_ctx, 1);
}

}  // namespace sys
