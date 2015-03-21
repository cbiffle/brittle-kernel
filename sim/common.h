#ifndef SIM_COMMON_H
#define SIM_COMMON_H

#include <cstdint>

static constexpr unsigned n_data = 4;

struct Message {
  uintptr_t data[n_data];  
};

inline bool operator==(Message const &a, Message const &b) {
  for (unsigned i = 0; i < n_data; ++i) {
    if (a.data[i] != b.data[i]) return false;
  }
  return true;
}

inline bool operator!=(Message const &a, Message const &b) {
  return !(a == b);
}

struct ReceivedMessage {
  uintptr_t port;
  Message m;
};

enum class SysResult {
  success = 0,
  refused,
  would_block,
};

#endif  // SIM_COMMON_H
