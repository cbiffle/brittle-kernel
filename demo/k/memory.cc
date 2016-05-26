#include "demo/k/memory.h"

#include "etl/assert.h"

#include "demo/runtime/ipc.h"

namespace memory {

void split(unsigned k, uint32_t pos, unsigned slot_key,
    unsigned top_key_out) {
  auto send_map = keymap(0, slot_key, 0, 0);
  auto recv_map = keymap(0, k, top_key_out, 0);

  auto rm = ipc({Descriptor::call(2, k), pos}, send_map, recv_map);
  ETL_ASSERT(!rm.m.desc.get_error());
}

void become(unsigned k, ObjectType ot, unsigned arg, unsigned arg_key) {
  auto send_map = keymap(0, arg_key, 0, 0);
  auto recv_map = keymap(0, k, 0, 0);

  auto rm = ipc({
      Descriptor::call(3, k),
      uint32_t(ot),
      arg,
      },
      send_map, recv_map);
  ETL_ASSERT(!rm.m.desc.get_error());
}

uint32_t peek(unsigned k, uint32_t offset) {
  auto rm = ipc({
      Descriptor::call(4, k),
      offset,
      }, 0, 0);
  ETL_ASSERT(!rm.m.desc.get_error());

  return rm.m.d0;
}

void poke(unsigned k, uint32_t offset, uint32_t word) {
  auto rm = ipc({
      Descriptor::call(5, k),
      offset,
      word,
      }, 0, 0);
  ETL_ASSERT(!rm.m.desc.get_error());
}

}  // namespace memory
