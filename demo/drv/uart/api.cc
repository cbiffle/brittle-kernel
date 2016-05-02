#include "demo/drv/uart/api.h"

#include "demo/runtime/ipc.h"

namespace demo {
namespace drv {
namespace uart {

bool send(unsigned k, uint8_t value) {
  auto rm = ipc({
      Descriptor::call(1, k),
      value,
    });

  return rm.m.desc.get_error() == false;
}

}  // namespace uart
}  // namespace drv
}  // namespace demo
