#include "a/k/object_table.h"

#include "etl/assert.h"
#include "a/rt/ipc.h"

namespace object_table {

rt::AutoKey mint_key(unsigned k, unsigned index, uint64_t brand) {
  auto k_out = rt::AutoKey{};
  auto recv_map = rt::keymap(0, k_out, 0, 0);
  ReceivedMessage rm = rt::ipc({
      Descriptor::call(0, k),
      index,
      uint32_t(brand),
      uint32_t(brand >> 32),
    }, 0, recv_map);

  ETL_ASSERT(!rm.m.desc.get_error());
  return k_out;
}

KeyInfo read_key(unsigned k, unsigned key) {
  auto send_map = rt::keymap(0, key);
  auto rm = rt::ipc({
      Descriptor::call(1, k),
      }, send_map, 0);
  
  ETL_ASSERT(!rm.m.desc.get_error());

  return {
    .index = rm.m.d0,
    .brand = rm.m.d1 | (Brand(rm.m.d2) << 32),
  };
}

Kind get_kind(unsigned k, unsigned index) {
  auto rm = rt::ipc({
      Descriptor::call(2, k),
      index
      }, 0, 0);
  
  ETL_ASSERT(!rm.m.desc.get_error());

  return Kind(rm.m.d0);
}

bool invalidate(unsigned k, unsigned index, bool rollover_ok) {
  auto rm = rt::ipc({
      Descriptor::call(3, k),
      index,
      rollover_ok
      }, 0, 0);
  
  return rm.m.desc.get_error() == false;
}

}  // namespace object_table
