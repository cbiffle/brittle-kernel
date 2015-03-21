#ifndef SIM_PROTOCOL_H
#define SIM_PROTOCOL_H

#include "common.h"

enum class RequestType : uint8_t {
  send,
  call,
  open_receive,
  copy_cap,
  null_cap,
  mask_port,
  unmask_port,
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

struct CopyCapRequest {
  uintptr_t from_index;
  uintptr_t to_index;
};

struct NullCapRequest {
  uintptr_t index;
};

struct MaskPortRequest {
  uintptr_t index;
};

struct UnmaskPortRequest {
  uintptr_t index;
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
