#include "a/k/object_table.h"

#include "etl/assert.h"
#include "a/rt/ipc.h"

namespace object_table {

rt::AutoKey mint_key(unsigned k, unsigned index, uint64_t brand) {
  Message msg {
    Descriptor::call(0, k),
    index,
    uint32_t(brand),
    uint32_t(brand >> 32),
  };
  auto k_out = rt::AutoKey{};
  rt::ipc2(msg,
      0,
      rt::keymap(0, k_out, 0, 0));
  ETL_ASSERT(!msg.desc.get_error());
  return k_out;
}

KeyInfo read_key(unsigned k, unsigned key) {
  Message msg {
    Descriptor::call(1, k),
  };
  rt::ipc2(msg,
      rt::keymap(0, key),
      0);
  ETL_ASSERT(!msg.desc.get_error());

  return {
    .index = msg.d0,
    .brand = msg.d1 | (Brand(msg.d2) << 32),
  };
}

Kind get_kind(unsigned k, unsigned index) {
  Message msg {
    Descriptor::call(2, k),
    index,
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());

  return Kind(msg.d0);
}

bool invalidate(unsigned k, unsigned index, bool rollover_ok) {
  Message msg {
    Descriptor::call(3, k),
    index,
    rollover_ok,
  };
  rt::ipc2(msg, 0, 0);
  return msg.desc.get_error() == false;
}

}  // namespace object_table
