#include "stubs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "protocol.h"

static constexpr bool trace = false;

/*******************************************************************************
 * Runtime
 */

template <typename T>
static void out(T const & data) {
  if (trace) fprintf(stderr, "writing %zu bytes\n", sizeof(data));
  if (write(1, &data, sizeof(data)) != sizeof(data)) {
    fprintf(stderr, "CHILD: write failed\n");
    exit(1);
  }
}

template <typename T>
static T in() {
  T tmp;
  if (trace) fprintf(stderr, "trying to read %zu bytes\n", sizeof(tmp));
  if (read(0, &tmp, sizeof(tmp)) != sizeof(tmp)) {
    fprintf(stderr, "CHILD: read failed\n");
    exit(1);
  }
  return tmp;
}

SysResult send(bool block, uintptr_t target, Message const & m) {
  out(RequestType::send);
  out(SendRequest {
      .blocking = block,
      .target = target,
      .m = m,
  });

  auto rt = in<ResponseType>();
  assert(rt == ResponseType::complete);
  auto r = in<CompleteResponse>();
  return r.result;
}

SysResult open_receive(bool block, ReceivedMessage * rm) {
  out(RequestType::open_receive);
  out(OpenReceiveRequest { .blocking = block });

  switch (in<ResponseType>()) {
    case ResponseType::message:
      {
        auto r = in<MessageResponse>();
        *rm = r.rm;
        return SysResult::success;
      }

    case ResponseType::complete:
      {
        auto r = in<CompleteResponse>();
        assert(r.result != SysResult::success);
        return r.result;
      }

    default:
      assert(false);
  }
}

SysResult call(bool block,
               uintptr_t target,
               Message const & m,
               ReceivedMessage * rm) {
  out(RequestType::call);
  out(SendRequest {
      .blocking = block,
      .target = target,
      .m = m,
  });

  switch (in<ResponseType>()) {
    case ResponseType::complete:
      {
        auto r = in<CompleteResponse>();
        assert(r.result != SysResult::success);
        return r.result;
      }

    case ResponseType::message:
      {
        auto r = in<MessageResponse>();
        *rm = r.rm;
        return SysResult::success;
      }

    default:
      assert(false);
  }
}

SysResult move_cap(uintptr_t from, uintptr_t to) {
  out(RequestType::copy_cap);
  out(CopyCapRequest {
      .from_index = from,
      .to_index = to,
    });
  auto rt = in<ResponseType>();
  assert(rt == ResponseType::complete);
  auto r = in<CompleteResponse>();
  return r.result;
}

SysResult mask(uintptr_t port) {
  out(RequestType::mask_port);
  out(MaskPortRequest { port });
  auto rt = in<ResponseType>();
  assert(rt == ResponseType::complete);
  auto r = in<CompleteResponse>();
  return r.result;
}

SysResult unmask(uintptr_t port) {
  out(RequestType::unmask_port);
  out(UnmaskPortRequest { port });
  auto rt = in<ResponseType>();
  assert(rt == ResponseType::complete);
  auto r = in<CompleteResponse>();
  return r.result;
}
