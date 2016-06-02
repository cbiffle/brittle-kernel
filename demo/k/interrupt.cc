#include "demo/k/interrupt.h"

#include "common/selectors.h"
#include "demo/runtime/ipc.h"

namespace S = selector::interrupt;

namespace interrupt {

bool set_target(unsigned k, unsigned target_key) {
  auto send_map = keymap(0, target_key, 0, 0);

  auto rm = ipc({Descriptor::call(S::set_target, k)}, send_map, 0);

  return rm.m.desc.get_error() == false;
}

bool enable(unsigned k, bool clear_pending) {
  auto rm = ipc({
      Descriptor::call(S::enable, k),
      clear_pending
    }, 0, 0);

  return rm.m.desc.get_error() == false;
}

}  // namespace interrupt
