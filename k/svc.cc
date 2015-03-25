#include "k/svc.h"

#include "etl/prediction.h"

#include "etl/data/maybe.h"
#include "etl/error/check.h"
#include "etl/error/ignore.h"

#include "etl/armv7m/registers.h"

#include "k/context.h"
#include "k/ipc.h"
#include "k/registers.h"
#include "k/sys_result.h"
#include "k/unprivileged.h"

namespace k {

static bool booted = false;

static SysResult svc_send() {
  auto target_index = CHECK(uload(&current->stack()->ef.r0));
  auto arg = reinterpret_cast<Message const *>(current->stack()->ef.r1);
  auto result = reinterpret_cast<Message *>(current->stack()->ef.r2);

  if (target_index >= config::n_task_keys) {
    return SysResult::bad_key_index;
  }

  return current->key(target_index).call(arg, result);
}

static SysResult normal_svc_dispatch() {
  auto r15 = CHECK(uload(&current->stack()->ef.r15));
  auto sysnum = CHECK(uload(reinterpret_cast<uint8_t const *>(r15 - 2)));
  switch (sysnum) {
    case 0:
      return svc_send();

    default:
      return SysResult::bad_svc;
  }
}

void * svc_dispatch(void * stack) {
  if (ETL_LIKELY(booted)) {
    // Normal invocation.
    current->set_stack(static_cast<Registers *>(stack));
    IGNORE(ustore(&current->stack()->ef.r0, unsigned(normal_svc_dispatch())));
  } else {
    // First syscall to start initial task.
    etl::armv7m::set_control(1);
    booted = true;
  }

  return current->stack();
}

}  // namespace k
