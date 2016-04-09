#ifndef DEMO_RUNTIME_IPC_H
#define DEMO_RUNTIME_IPC_H

#include <cstdint>

#include "etl/attribute_macros.h"
#include "common/descriptor.h"
#include "common/message.h"

ReceivedMessage ipc(Message const &);

ETL_INLINE
void copy_key(unsigned to, unsigned from) {
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

#endif  // DEMO_RUNTIME_IPC_H
