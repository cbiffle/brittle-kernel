#include "demo/k/memory.h"

#include "etl/assert.h"

#include "demo/runtime/ipc.h"

namespace memory {

void split(unsigned k, unsigned slot_key,
    unsigned top_key_out) {
  copy_key(1, slot_key);

  auto rm = ipc({Descriptor::call(2, k)});
  ETL_ASSERT(!rm.m.d0.get_error());

  copy_key(k, 1);
  copy_key(top_key_out, 2);
}

void become(unsigned k, ObjectType ot, unsigned arg, unsigned arg_key) {
  if (arg_key) copy_key(1, arg_key);

  auto rm = ipc({
      Descriptor::call(3, k),
      uint32_t(ot),
      arg,
      });
  ETL_ASSERT(!rm.m.d0.get_error());

  copy_key(k, 1);
}

uint32_t peek(unsigned k, uint32_t offset) {
  auto rm = ipc({
      Descriptor::call(4, k),
      offset,
      });
  ETL_ASSERT(!rm.m.d0.get_error());

  return rm.m.d1;
}

void poke(unsigned k, uint32_t offset, uint32_t word) {
  auto rm = ipc({
      Descriptor::call(5, k),
      offset,
      word,
      });
  ETL_ASSERT(!rm.m.d0.get_error());
}

}  // namespace memory
