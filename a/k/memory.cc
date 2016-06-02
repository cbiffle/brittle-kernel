#include "a/k/memory.h"

#include "etl/assert.h"

#include "common/selectors.h"
#include "a/rt/ipc.h"

namespace S = selector::memory;

namespace memory {

Region inspect(unsigned k) {
  auto msg = Message {Descriptor::call(S::inspect, k)};
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
  return {
    .rbar = msg.d0,
    .rasr = msg.d1,
  };
}

rt::AutoKey split(unsigned k, uint32_t pos, unsigned slot_key) {
  Message msg {
      Descriptor::call(S::split, k),
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
    Descriptor::call(S::become, k),
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
      Descriptor::call(S::peek, k),
      offset,
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
  return msg.d0;
}

void poke(unsigned k, uint32_t offset, uint32_t word) {
  Message msg {
    Descriptor::call(S::poke, k),
    offset,
    word,
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
}

void make_child(unsigned k, uintptr_t base, size_t size, unsigned slot_key) {
  Message msg {
    Descriptor::call(S::make_child, k),
    base,
    size,
  };
  rt::ipc2(msg,
      rt::keymap(0, slot_key, 0, 0),
      rt::keymap(0, slot_key, 0, 0));
  ETL_ASSERT(!msg.desc.get_error());
}

}  // namespace memory
