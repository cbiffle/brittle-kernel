#include "a/rt/ipc.h"

static_assert(sizeof(Message) == 6 * sizeof(uint32_t),
    "This code expects a six-word Message.");

static_assert(sizeof(ReceivedMessage) == 8 * sizeof(uint32_t),
    "This code expects an eight-word ReceivedMessage.");

namespace rt {

ReceivedMessage blocking_receive(unsigned k, uint32_t recv_map) {
  return ipc({
      Descriptor::zero()
        .with_receive_enabled(true)
        .with_source(k)
        .with_block(true)},
      0,
      recv_map);
}

}  // namespace rt
