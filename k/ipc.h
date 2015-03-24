#ifndef K_IPC_H
#define K_IPC_H

#include <stdint.h>

#include "k/config.h"

namespace k {

struct Message {
  uint32_t data[config::n_message_data];
};

}  // namespace k

#endif  // K_IPC_H
