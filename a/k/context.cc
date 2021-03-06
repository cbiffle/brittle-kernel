#include "a/k/context.h"

#include "etl/assert.h"

#include "common/selectors.h"
#include "a/rt/ipc.h"

namespace S = selector::context;

namespace context {

void set_region(unsigned k, unsigned region_index, unsigned region_key) {
  Message msg {
    Descriptor::call(S::write_region_register, k),
    region_index,
  };
  rt::ipc2(msg,
      rt::keymap(0, region_key, 0, 0),
      0);
  ETL_ASSERT(!msg.desc.get_error());
}

rt::AutoKey get_region(unsigned k, unsigned region_index) {
  Message msg {
    Descriptor::call(S::read_region_register, k),
    region_index,
  };
  auto k_out = rt::AutoKey{};
  rt::ipc2(msg,
      0,
      rt::keymap(0, k_out, 0, 0));
  ETL_ASSERT(!msg.desc.get_error());

  return k_out;
}

void set_register(unsigned k, Register r, uint32_t value) {
  Message msg {
    Descriptor::call(S::write_register, k),
    uint32_t(r),
    value,
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
}

void set_key(unsigned k, unsigned index, unsigned source_index) {
  Message msg {
    Descriptor::call(S::write_key_register, k),
    index,
  };
  rt::ipc2(msg,
      rt::keymap(0, source_index, 0, 0),
      0);
  ETL_ASSERT(!msg.desc.get_error());
}

void make_runnable(unsigned k) {
  Message msg {
    Descriptor::call(S::make_runnable, k),
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
}

void set_priority(unsigned k, unsigned priority) {
  Message msg {
    Descriptor::call(S::set_priority, k),
    priority,
  };
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(!msg.desc.get_error());
}

}  // namespace context
