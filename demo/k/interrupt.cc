#include "demo/k/interrupt.h"

#include "demo/runtime/ipc.h"

namespace interrupt {

bool set_target(unsigned k, unsigned target_key) {
  copy_key(1, target_key);

  auto rm = ipc({Descriptor::call(1, k)});

  return rm.m.d0.get_error() == false;
}

bool enable(unsigned k, bool clear_pending) {
  auto rm = ipc({
      Descriptor::call(2, k),
      clear_pending
    });

  return rm.m.d0.get_error() == false;
}

}  // namespace interrupt
