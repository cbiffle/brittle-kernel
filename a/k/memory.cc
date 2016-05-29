#include "a/k/memory.h"

#include "etl/assert.h"

#include "a/rt/ipc.h"

namespace memory {

Region inspect(unsigned k) {
  auto msg = Message {Descriptor::call(0, k)};
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
  return {
    .rbar = msg.d0,
    .rasr = msg.d1,
  };
}

rt::AutoKey split(unsigned k, uint32_t pos, unsigned slot_key) {
  Message msg {
      Descriptor::call(2, k),
      pos,
  };
  auto k_top = rt::AutoKey{};
  rt::ipc2(msg,
      rt::keymap(0, slot_key, 0, 0),
      rt::keymap(0, k, k_top, 0));
  ETL_ASSERT(!msg.desc.get_error());

  return k_top;
}

void become(unsigned k, ObjectType ot, unsigned arg, unsigned arg_key) {
  Message msg {
    Descriptor::call(3, k),
    uint32_t(ot),
    arg,
  };
  rt::ipc2(msg,
      rt::keymap(0, arg_key, 0, 0),
      rt::keymap(0, k, 0, 0));
  ETL_ASSERT(!msg.desc.get_error());
}

uint32_t peek(unsigned k, uint32_t offset) {
  Message msg {
      Descriptor::call(4, k),
      offset,
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
  return msg.d0;
}

void poke(unsigned k, uint32_t offset, uint32_t word) {
  Message msg {
    Descriptor::call(5, k),
    offset,
    word,
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
}

}  // namespace memory
