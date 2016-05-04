#ifndef DEMO_RUNTIME_IPC_H
#define DEMO_RUNTIME_IPC_H

#include <cstdint>

#include "etl/attribute_macros.h"
#include "common/descriptor.h"
#include "common/message.h"

ReceivedMessage ipc(Message const &,
                    uint32_t send_map = 0x3210,
                    uint32_t receive_map = 0x3210);

ETL_INLINE
void copy_key(unsigned to, unsigned from) {
  if (to == from) return;

  auto d = Descriptor::zero()
    .with_sysnum(1)
    .with_source(from)
    .with_target(to);

  register auto d_bits asm ("r4") = uint32_t(d);

  asm volatile(
      "svc #0"
      : /* no outputs */
      : "r"(d_bits)
      );
}

ETL_INLINE
void discard_keys(unsigned first, unsigned last) {
  auto d = Descriptor::zero()
    .with_sysnum(2)
    .with_source(first)
    .with_target(last);

  register auto d_bits asm ("r4") = uint32_t(d);

  asm volatile(
      "svc #0"
      : /* no outputs */
      : "r"(d_bits)
      );
}

ETL_INLINE
void discard_received_keys() {
  discard_keys(0, 3);
}

#endif  // DEMO_RUNTIME_IPC_H
