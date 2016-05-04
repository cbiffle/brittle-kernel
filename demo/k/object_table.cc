#include "demo/k/object_table.h"

#include "demo/runtime/ipc.h"

namespace object_table {

bool mint_key(unsigned k, unsigned index, uint64_t brand, unsigned out_index) {
  auto recv_map = keymap(0, out_index, 0, 0);
  ReceivedMessage rm = ipc({
      Descriptor::call(0, k),
      index,
      uint32_t(brand),
      uint32_t(brand >> 32),
    }, 0, recv_map);

  return !rm.m.desc.get_error();
}

}  // namespace object_table
