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

static void normal_svc_dispatch() {
  // The fact that we're here in an SVC, instead of escalated to a
  // MemMang fault, means that the exception frame falls within
  // valid user memory.  So we can use direct access to get r15.
  auto r15 = current->stack()->r15;

  // Similarly, the negative displacement we apply to r15 to get
  // the sysnum should ensure that the address is valid.
  auto sysnum = *reinterpret_cast<uint8_t const *>(r15 - 2);

  SysResult result;

  switch (sysnum) {
    case 0:  // Send Message
      result = current->do_send(false);
      break;

    case 1:  // Call
      result = current->do_send(true);
      break;

    case 2:  // Move Key
      {
        auto to = current->stack()->r0 % config::n_task_keys;
        auto from = current->stack()->r1 % config::n_task_keys;
        current->key(to) = current->key(from);
      }
      break;

    default:
      result = SysResult::bad_svc;
      break;
  }

  if (result != SysResult::success) {
    // Attempt to report to the caller that they have screwed up.
    // Should this fault, we just tolerate it.
    IGNORE(ustore(&current->stack()->r0, unsigned(result)));
  } else {
    // If the svc impl returns success, it's signalling that it has
    // handled caller reporting -- we mustn't do it again.
  }
}

void * svc_dispatch(void * stack) {
  if (ETL_LIKELY(booted)) {
    // Normal invocation.
    current->set_stack(static_cast<Registers *>(stack));
    normal_svc_dispatch();
  } else {
    // First syscall to start initial task.
    etl::armv7m::set_control(1);
    booted = true;
  }

  return current->stack();
}

}  // namespace k
