#include "a/k/interrupt.h"

#include "etl/assert.h"
#include "a/rt/ipc.h"

namespace interrupt {

void set_target(unsigned k, unsigned target_key) {
  Message msg {Descriptor::call(1, k)};
  rt::ipc2(msg, rt::keymap(0, target_key, 0, 0), 0);
  ETL_ASSERT(msg.desc.get_error() == false);
}

void enable(unsigned k, bool clear_pending) {
  Message msg {Descriptor::call(2, k), clear_pending};
  rt::ipc2(msg, 0, 0);
  ETL_ASSERT(msg.desc.get_error() == false);
}

}  // namespace interrupt
