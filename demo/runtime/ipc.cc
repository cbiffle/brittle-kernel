#include "demo/runtime/ipc.h"

static_assert(sizeof(Message) == 4 * sizeof(uint32_t),
    "This code expects a four-word Message.");

static_assert(sizeof(ReceivedMessage) == 5 * sizeof(uint32_t),
    "This code expects a five-word ReceivedMessage.");

__attribute__((naked))
ReceivedMessage ipc(Message const &) {
  asm volatile (
      "push {r4-r8, lr}\n"
      "ldm r1, {r4, r5, r6, r7}\n"
      "svc #0\n"
      "stm r0, {r4, r5, r6, r7, r8}\n"
      "pop {r4-r8, pc}\n"
      );
}
