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

}  // namespace sys
