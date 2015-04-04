#ifndef K_IPC_H
#define K_IPC_H

#include <stdint.h>

#include "k/config.h"
#include "k/sys_result.h"
#include "k/types.h"

namespace k {

struct Message {
  constexpr Message(uint32_t d0 = 0,
                    uint32_t d1 = 0,
                    uint32_t d2 = 0,
                    uint32_t d3 = 0)
    : data{d0, d1, d2, d3} {}

  uint32_t data[config::n_message_data];
};

__attribute__((warn_unused_result))
SysResult ustore(Message *, Message const &);

__attribute__((warn_unused_result))
SysResultWith<Message> uload(Message const *);

struct ReceivedMessage {
  Brand gate_brand;
  Brand sender_brand;
  Message m;
};

}  // namespace k

#endif  // K_IPC_H
