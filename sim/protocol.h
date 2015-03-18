#ifndef SIM_PROTOCOL_H
#define SIM_PROTOCOL_H

#include <cstdint>

enum class TaskMessageType : uint8_t {
  send,
  send_nb,
  call,
  open_receive,
};

struct Message {
  unsigned target;
  unsigned mdata[4];
};

inline bool operator==(Message const &a, Message const &b) {
  if (a.target != b.target) return false;
  for (unsigned i = 0; i < 4; ++i) {
    if (a.mdata[i] != b.mdata[i]) return false;
  }
  return true;
}


enum class SysMessageType : uint8_t {
  done_basic,
  done_message,
};

struct BasicResponse {
  unsigned status;
};

struct MessageResponse {
  unsigned status;
  unsigned brand;
  unsigned mdata[4];
};

#endif  // SIM_PROTOCOL_H
