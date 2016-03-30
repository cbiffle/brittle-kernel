#include "demo/driver/api.h"

#include "demo/runtime/ipc.h"

namespace uart {

bool send(unsigned k, uint8_t value) {
  auto rm = ipc({
      Descriptor::call(1, k),
      value,
    });

  return rm.m.d0.get_error() == false;
}

}  // namespace uart
