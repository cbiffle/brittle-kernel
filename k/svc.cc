#include "k/svc.h"

#include "etl/prediction.h"

#include "etl/data/maybe.h"
#include "etl/error/check.h"

#include "etl/armv7m/registers.h"

#include "k/context.h"
#include "k/ipc.h"
#include "k/registers.h"
#include "k/sys_result.h"

namespace k {

static bool booted = false;

static bool svc_send() {
  auto target_index = current->stack->ef.r0;
  auto message = reinterpret_cast<Message const *>(current->stack->ef.r1);

  if (target_index >= config::n_task_keys) {
    current->stack->ef.r0 = unsigned(SysResult::bad_key_index);
    return false;
  }

  current->stack->ef.r0 = unsigned(current->keys[target_index].send(message));
  return false;
}

static bool normal_svc_dispatch() {
  auto r15 = current->stack->ef.r15;
  auto sysnum = *reinterpret_cast<uint8_t const *>(r15 - 2);
  switch (sysnum) {
    case 0:
      return svc_send();

    default:
      current->stack->ef.r0 = unsigned(SysResult::bad_svc);
      return false;
  }
}

void * svc_dispatch(void * stack) {
  if (ETL_LIKELY(booted)) {
    // Normal invocation.
    current->stack = static_cast<Registers *>(stack);
    if (!normal_svc_dispatch()) {
      current->stack->ef.r0 = unsigned(SysResult::fault);
    }
  } else {
    // First syscall to start initial task.
    etl::armv7m::set_control(1);
    booted = true;
  }

  return current->stack;
}

}  // namespace k
