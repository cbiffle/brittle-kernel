#include "a/sys/api.h"

#include "etl/assert.h"
#include "a/rt/ipc.h"
#include "a/sys/selectors.h"

namespace sys {

void map_memory(unsigned sys_key, unsigned mem_key, unsigned index) {
  Message msg {
    Descriptor::call(selector::map_memory, sys_key),
    index,
  };
  rt::ipc2(msg, rt::keymap(0, mem_key, 0, 0), 0);
  ETL_ASSERT(msg.desc.get_error() == false);
}

void exit(unsigned sys_key, ExitReason reason,
                            uint32_t d1,
                            uint32_t d2,
                            uint32_t d3,
                            uint32_t d4) {
  // Wrap the whole invocation in a loop.  If the system misbehaves and restarts
  // us after we've tried to exit, we'll just reissue the call.
  while (true) {
    Message msg {
      Descriptor::call(selector::exit, sys_key),
      uint32_t(reason), d1, d2, d3, d4,
    };
    rt::ipc2(msg, 0, 0);
  }
}

}  // namespace sys
