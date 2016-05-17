#include "a/k/interrupt.h"

#include "etl/assert.h"
#include "a/rt/ipc.h"

namespace interrupt {

void set_target(unsigned k, unsigned target_key) {
  auto send_map = rt::keymap(0, target_key, 0, 0);

  auto rm = rt::ipc({Descriptor::call(1, k)}, send_map, 0);

  ETL_ASSERT(rm.m.desc.get_error() == false);
}

void enable(unsigned k, bool clear_pending) {
  auto rm = rt::ipc({
      Descriptor::call(2, k),
      clear_pending
    }, 0, 0);

  ETL_ASSERT(rm.m.desc.get_error() == false);
}

}  // namespace interrupt
