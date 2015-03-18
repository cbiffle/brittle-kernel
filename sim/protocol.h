#ifndef SIM_PROTOCOL_H
#define SIM_PROTOCOL_H

#include "common.h"

enum class RequestType : uint8_t {
  send,
  call,
  open_receive,
};

struct SendRequest {
  bool blocking;
  uintptr_t target;
  Message m;
};

struct CallRequest {
  bool blocking;
  uintptr_t target;
  Message m;
};

struct OpenReceiveRequest {
  bool blocking;
};


enum class ResponseType : uint8_t {
  complete,
  message,
};

struct CompleteResponse {
  SysResult result;
};

struct MessageResponse {
  ReceivedMessage rm;
};

#endif  // SIM_PROTOCOL_H
