#include "demo/context.h"

#include "demo/ipc.h"

namespace context {

bool set_region(unsigned k, unsigned region_index, unsigned region_key) {
  move_key(1, region_key);

  ReceivedMessage rm;
  if (call(k, {5, region_index, 0, 0}, &rm)) return false;

  return rm.m.data[0];
}

}  // namespace context
