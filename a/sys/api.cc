#include "a/sys/api.h"

#include "etl/assert.h"
#include "a/rt/ipc.h"
#include "a/sys/selectors.h"

namespace sys {

void map_memory(unsigned sys_key, unsigned mem_key, unsigned index) {
  auto send_map = rt::keymap(0, mem_key, 0, 0);

  auto rm = rt::ipc({
      Descriptor::call(selector::map_memory, sys_key),
      index,
    }, send_map, 0);

  ETL_ASSERT(rm.m.desc.get_error() == false);
}

}  // namespace sys
