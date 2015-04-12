#include "demo/k/context.h"

#include "demo/runtime/ipc.h"

namespace context {

bool set_region(unsigned k, unsigned region_index, unsigned region_key) {
  copy_key(1, region_key);

  auto rm = ipc({
      Descriptor::call(5, k),
      region_index,
    });

  return rm.m.d0.get_error() == false;
}

bool get_region(unsigned k, unsigned region_index, unsigned region_key_out) {

  auto rm = ipc({
      Descriptor::call(4, k),
      region_index,
    });

  if (rm.m.d0.get_error() == false) {
    copy_key(region_key_out, 1);
    return true;
  } else {
    return false;
  }
}

bool set_register(unsigned k, unsigned index, uint32_t value) {
  auto rm = ipc({
      Descriptor::call(1, k),
      index,
      value,
    });

  return rm.m.d0.get_error() == false;
}

bool set_key(unsigned k, unsigned index, unsigned source_index) {
  copy_key(1, source_index);

  auto rm = ipc({
      Descriptor::call(3, k),
      index,
    });

  return rm.m.d0.get_error() == false;
}

bool make_runnable(unsigned k) {
  auto rm = ipc({Descriptor::call(6, k)});
  return rm.m.d0.get_error() == false;
}

}  // namespace context
