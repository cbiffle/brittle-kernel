#include "demo/ipc.h"

__attribute__((naked))
unsigned call(unsigned index, Message const & m, ReceivedMessage * out) {
  asm volatile (
      "svc #1\n"
      "bx lr\n"
      );
}

__attribute__((naked))
void move_key(unsigned to, unsigned from) {
  asm volatile(
      "svc #2\n"
      "bx lr\n"
      );
}
