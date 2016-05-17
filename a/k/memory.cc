#include "a/k/memory.h"

#include "etl/assert.h"

#include "a/rt/ipc.h"

namespace memory {

Region inspect(unsigned k) {
  auto rm = rt::ipc({Descriptor::call(0, k)}, 0, 0);
  ETL_ASSERT(!rm.m.desc.get_error());
  return {
    .rbar = rm.m.d0,
    .rasr = rm.m.d1,
  };
}

rt::AutoKey split(unsigned k, uint32_t pos, unsigned slot_key) {
  auto k_top = rt::AutoKey{};
  auto send_map = rt::keymap(0, slot_key, 0, 0);
  auto recv_map = rt::keymap(0, k, k_top, 0);

  auto rm = rt::ipc({
      Descriptor::call(2, k),
      pos,
    }, send_map, recv_map);
  ETL_ASSERT(!rm.m.desc.get_error());

  return k_top;
}

void become(unsigned k, ObjectType ot, unsigned arg, unsigned arg_key) {
  auto send_map = rt::keymap(0, arg_key, 0, 0);
  auto recv_map = rt::keymap(0, k, 0, 0);

  auto rm = rt::ipc({
      Descriptor::call(3, k),
      uint32_t(ot),
      arg,
      },
      send_map, recv_map);
  ETL_ASSERT(!rm.m.desc.get_error());
}

uint32_t peek(unsigned k, uint32_t offset) {
  auto rm = rt::ipc({
      Descriptor::call(4, k),
      offset,
      }, 0, 0);
  ETL_ASSERT(!rm.m.desc.get_error());

  return rm.m.d0;
}

void poke(unsigned k, uint32_t offset, uint32_t word) {
  auto rm = rt::ipc({
      Descriptor::call(5, k),
      offset,
      word,
      }, 0, 0);
  ETL_ASSERT(!rm.m.desc.get_error());
}

}  // namespace memory
