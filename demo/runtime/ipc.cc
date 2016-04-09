#include "demo/runtime/ipc.h"

__attribute__((naked))
ReceivedMessage ipc(Message const &) {
  asm volatile (
      "push {r4-r9, lr}\n"
      "ldm r1, {r4, r5, r6, r7}\n"
      "svc #0\n"
      "stm r0, {r4, r5, r6, r7, r8, r9}\n"
      "pop {r4-r9, pc}\n"
      );
}
