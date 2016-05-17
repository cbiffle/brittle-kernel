#include "a/k/context.h"

#include "etl/assert.h"

#include "a/rt/ipc.h"

namespace context {

void set_region(unsigned k, unsigned region_index, unsigned region_key) {
  auto send_map = rt::keymap(0, region_key, 0, 0);

  auto rm = rt::ipc({
      Descriptor::call(5, k),
      region_index,
    }, send_map, 0);

  ETL_ASSERT(!rm.m.desc.get_error());
}

rt::AutoKey get_region(unsigned k, unsigned region_index) {
  auto k_out = rt::AutoKey{};
  auto recv_map = rt::keymap(0, k_out, 0, 0);

  auto rm = rt::ipc({
      Descriptor::call(4, k),
      region_index,
    }, 0, recv_map);

  ETL_ASSERT(!rm.m.desc.get_error());

  return k_out;
}

void set_register(unsigned k, Register r, uint32_t value) {
  auto rm = rt::ipc({
      Descriptor::call(1, k),
      uint32_t(r),
      value,
    }, 0, 0);

  ETL_ASSERT(!rm.m.desc.get_error());
}

void set_key(unsigned k, unsigned index, unsigned source_index) {
  auto send_map = rt::keymap(0, source_index, 0, 0);

  auto rm = rt::ipc({
      Descriptor::call(3, k),
      index,
    }, send_map, 0);

  ETL_ASSERT(!rm.m.desc.get_error());
}

void make_runnable(unsigned k) {
  auto rm = rt::ipc({Descriptor::call(6, k)}, 0, 0);

  ETL_ASSERT(!rm.m.desc.get_error());
}

void set_priority(unsigned k, unsigned priority) {
  auto rm = rt::ipc({
      Descriptor::call(8, k),
      priority,
    }, 0, 0);

  ETL_ASSERT(!rm.m.desc.get_error());
}

}  // namespace context
