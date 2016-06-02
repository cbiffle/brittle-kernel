#include "demo/k/context.h"

#include "common/selectors.h"
#include "demo/runtime/ipc.h"

namespace S = selector::context;

namespace context {

bool set_region(unsigned k, unsigned region_index, unsigned region_key) {
  auto send_map = keymap(0, region_key, 0, 0);

  auto rm = ipc({
      Descriptor::call(S::write_region_register, k),
      region_index,
    }, send_map, 0);

  return rm.m.desc.get_error() == false;
}

bool get_region(unsigned k, unsigned region_index, unsigned region_key_out) {
  auto recv_map = keymap(0, region_key_out, 0, 0);

  auto rm = ipc({
      Descriptor::call(S::read_region_register, k),
      region_index,
    }, 0, recv_map);

  return !rm.m.desc.get_error();
}

bool set_register(unsigned k, Register r, uint32_t value) {
  auto rm = ipc({
      Descriptor::call(S::write_register, k),
      uint32_t(r),
      value,
    }, 0, 0);

  return rm.m.desc.get_error() == false;
}

bool set_key(unsigned k, unsigned index, unsigned source_index) {
  auto send_map = keymap(0, source_index, 0, 0);

  auto rm = ipc({
      Descriptor::call(S::write_key_register, k),
      index,
    }, send_map, 0);

  return rm.m.desc.get_error() == false;
}

bool make_runnable(unsigned k) {
  auto rm = ipc({Descriptor::call(S::make_runnable, k)}, 0, 0);
  return rm.m.desc.get_error() == false;
}

bool set_priority(unsigned k, unsigned priority) {
  auto rm = ipc({
      Descriptor::call(S::set_priority, k),
      priority,
    }, 0, 0);

  return rm.m.desc.get_error() == false;
}

}  // namespace context
