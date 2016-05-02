#include "demo/runtime/ipc.h"

static_assert(sizeof(Message) == 5 * sizeof(uint32_t),
    "This code expects a five-word Message.");

static_assert(sizeof(ReceivedMessage) == 6 * sizeof(uint32_t),
    "This code expects a six-word ReceivedMessage.");

__attribute__((naked))
ReceivedMessage ipc(Message const &) {
  asm volatile (
      "push {r4-r9, lr}\n"
      "ldm r1, {r4, r5, r6, r7, r8}\n"
      "svc #0\n"
      "stm r0, {r4, r5, r6, r7, r8, r9}\n"
      "pop {r4-r9, pc}\n"
      );
}
