#include "demo/context.h"

#include "demo/ipc.h"

namespace context {

bool set_region(unsigned k, unsigned region_index, unsigned region_key) {
  copy_key(1, region_key);

  auto rm = ipc({
      Descriptor::call(5, k),
      region_index,
    });

  return rm.m.d0.get_error() == false;
}

}  // namespace context
