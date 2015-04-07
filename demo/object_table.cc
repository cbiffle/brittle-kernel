#include "demo/object_table.h"

#include "demo/ipc.h"

namespace object_table {

bool mint_key(unsigned k, unsigned index, uint64_t brand, unsigned out_index) {
  ReceivedMessage rm = ipc({
      Descriptor::call(0, k),
      index,
      uint32_t(brand),
      uint32_t(brand >> 32),
    });

  if (rm.m.d0.get_error()) return false;

  copy_key(out_index, 1);
  return true;
}

}  // namespace object_table
