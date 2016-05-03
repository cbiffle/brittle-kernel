#include "demo/runtime/ipc.h"

static_assert(sizeof(Message) == 6 * sizeof(uint32_t),
    "This code expects a six-word Message.");

static_assert(sizeof(ReceivedMessage) == 7 * sizeof(uint32_t),
    "This code expects a seven-word ReceivedMessage.");

__attribute__((naked))
ReceivedMessage ipc(Message const &) {
  asm volatile (
      "push {r4-r10, lr}\n"
      "ldm r1, {r4, r5, r6, r7, r8, r9}\n"
      "svc #0\n"
      "stm r0, {r4, r5, r6, r7, r8, r9, r10}\n"
      "pop {r4-r10, pc}\n"
      );
}
