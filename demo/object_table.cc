#include "demo/object_table.h"

#include "demo/ipc.h"

namespace object_table {

bool mint_key(unsigned k, unsigned index, uint64_t brand, unsigned out_index) {
  ReceivedMessage rm;
  if (call(k, {0, index, uint32_t(brand), uint32_t(brand >> 32)}, &rm)) {
    return false;
  }

  move_key(out_index, 0);
  return true;
}

}  // namespace object_table
