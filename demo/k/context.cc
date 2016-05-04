#include "demo/k/context.h"

#include "demo/runtime/ipc.h"

namespace context {

bool set_region(unsigned k, unsigned region_index, unsigned region_key) {
  auto send_map = keymap(0, region_key, 0, 0);

  auto rm = ipc({
      Descriptor::call(5, k),
      region_index,
    }, send_map, 0);

  return rm.m.desc.get_error() == false;
}

bool get_region(unsigned k, unsigned region_index, unsigned region_key_out) {
  auto recv_map = keymap(0, region_key_out, 0, 0);

  auto rm = ipc({
      Descriptor::call(4, k),
      region_index,
    }, 0, recv_map);

  return !rm.m.desc.get_error();
}

bool set_register(unsigned k, Register r, uint32_t value) {
  auto rm = ipc({
      Descriptor::call(1, k),
      uint32_t(r),
      value,
    }, 0, 0);

  return rm.m.desc.get_error() == false;
}

bool set_key(unsigned k, unsigned index, unsigned source_index) {
  auto send_map = keymap(0, source_index, 0, 0);

  auto rm = ipc({
      Descriptor::call(3, k),
      index,
    }, send_map, 0);

  return rm.m.desc.get_error() == false;
}

bool make_runnable(unsigned k) {
  auto rm = ipc({Descriptor::call(6, k)}, 0, 0);
  return rm.m.desc.get_error() == false;
}

bool set_priority(unsigned k, unsigned priority) {
  auto rm = ipc({
      Descriptor::call(8, k),
      priority,
    }, 0, 0);

  return rm.m.desc.get_error() == false;
}

}  // namespace context
