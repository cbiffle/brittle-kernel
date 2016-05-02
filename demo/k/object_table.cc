#include "demo/k/object_table.h"

#include "demo/runtime/ipc.h"

namespace object_table {

bool mint_key(unsigned k, unsigned index, uint64_t brand, unsigned out_index) {
  discard_received_keys();
  ReceivedMessage rm = ipc({
      Descriptor::call(0, k),
      index,
      uint32_t(brand),
      uint32_t(brand >> 32),
    });

  if (rm.m.desc.get_error()) return false;

  copy_key(out_index, 1);
  return true;
}

}  // namespace object_table
