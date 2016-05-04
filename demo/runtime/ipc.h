#ifndef DEMO_RUNTIME_IPC_H
#define DEMO_RUNTIME_IPC_H

#include <cstdint>

#include "etl/attribute_macros.h"
#include "common/descriptor.h"
#include "common/message.h"

ReceivedMessage ipc(Message const &,
                    uint32_t send_map,
                    uint32_t receive_map);

static constexpr uint32_t keymap(unsigned k0,
                                 unsigned k1,
                                 unsigned k2,
                                 unsigned k3) {
  return (k0 & 0xF)
       | ((k1 & 0xF) << 4)
       | ((k2 & 0xF) << 8)
       | ((k3 & 0xF) << 12);
}

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

#endif  // DEMO_RUNTIME_IPC_H
