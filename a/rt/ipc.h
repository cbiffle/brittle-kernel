#ifndef A_RT_IPC_H
#define A_RT_IPC_H

#include <cstdint>

#include "etl/attribute_macros.h"
#include "common/descriptor.h"
#include "common/message.h"
#include "common/sysnums.h"

namespace rt {

ReceivedMessage ipc(Message const &,
                    uint32_t send_map,
                    uint32_t receive_map);

void ipc2(Message &,
          uint32_t send_map,
          uint32_t receive_map,
          uint64_t * brand_out = nullptr);

static constexpr uint32_t keymap(unsigned k0 = 0,
                                 unsigned k1 = 0,
                                 unsigned k2 = 0,
                                 unsigned k3 = 0) {
  return (k0 & 0xF)
       | ((k1 & 0xF) << 4)
       | ((k2 & 0xF) << 8)
       | ((k3 & 0xF) << 12);
}

ETL_INLINE
void copy_key(unsigned to, unsigned from) {
  if (to == from) return;

  auto d = Descriptor::zero()
    .with_sysnum(kabi::sysnum_copy_key)
    .with_source(from)
    .with_target(to);

  register auto d_bits asm ("r4") = uint32_t(d);

  asm volatile(
      "svc #0"
      : /* no outputs */
      : "r"(d_bits)
      );
}

ReceivedMessage blocking_receive(unsigned k, uint32_t recv_map);

}  // namespace rt

#endif  // A_RT_IPC_H
