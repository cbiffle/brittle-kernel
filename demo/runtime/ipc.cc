#include "demo/runtime/ipc.h"

__attribute__((naked))
ReceivedMessage ipc(Message const &) {
  asm volatile (
      "push {r4-r11, lr}\n"
      "ldm r1, {r4, r5, r6, r7}\n"
      "svc #0\n"
      "stm r0, {r4, r5, r6, r7, r8, r9}\n"
      "pop {r4-r11, pc}\n"
      );
}

__attribute__((naked))
void copy_key(unsigned to, unsigned from) {
  asm volatile(
      "push {r4, lr}\n"
      "lsls r4, r0, #20\n"          // Shift "to" into target field
      "orrs r4, r4, r1, lsl #24\n"  // Combine "from" in source field
      "orrs r4, r4, #(1 << 28)\n"   // Add sysnum
      "svc #0\n"
      "pop {r4, pc}\n"
      );
}
