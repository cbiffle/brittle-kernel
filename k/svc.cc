#include "k/svc.h"

#include "etl/prediction.h"

#include "etl/armv7m/registers.h"

#include "k/context.h"
#include "k/ipc.h"
#include "k/registers.h"
#include "k/sys_result.h"
#include "k/unprivileged.h"

namespace k {

static bool booted = false;

void * svc_dispatch(void * stack) {
  if (ETL_LIKELY(booted)) {
    // Normal invocation.
    current->set_stack(static_cast<StackRegisters *>(stack));
    current->do_syscall();
  } else {
    // First syscall to start initial task.
    etl::armv7m::set_control(1);
    booted = true;
  }

  return current->stack();
}

}  // namespace k
