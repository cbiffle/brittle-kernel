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
  // r0 and r1, as part of the exception frame, can be accessed
  // without protection -- we haven't yet allowed for a race that
  // could cause them to become inaccessible.
  auto target_index = current->stack()->ef.r0;
  auto arg = reinterpret_cast<Message const *>(current->stack()->ef.r1);

  if (target_index >= config::n_task_keys) {
    return SysResult::bad_key_index;
  }

  // Alright, we're handing off control to some object.  From
  // this point forward we must be more careful with our
  // accesses.  In particular, it is no longer our job (as the
  // dispatcher) to report the result through to the caller;
  // for all we know, the caller is deleted before this returns!
  // So, we must expose success to the dispatcher, suppressing
  // its reporting behavior.
  // However, we cannot simply let errors go unreported, nor can
  // we expect every possible implementation of deliver_from to
  // remember to call complete_send.  So we provide it here.

  auto r = current->key(target_index).deliver_from(current);
  if (r != SysResult::success) current->complete_send(r);
  return SysResult::success;
}

static void normal_svc_dispatch() {
  // The fact that we're here in an SVC, instead of escalated to a
  // MemMang fault, means that the exception frame falls within
  // valid user memory.  So we can use direct access to get r15.
  auto r15 = current->stack()->ef.r15;

  // Similarly, the negative displacement we apply to r15 to get
  // the sysnum should ensure that the address is valid.
  auto sysnum = *reinterpret_cast<uint8_t const *>(r15 - 2);

  SysResult result;

  switch (sysnum) {
    case 0:
      result = svc_send();
      break;

    default:
      result = SysResult::bad_svc;
      break;
  }

  if (result != SysResult::success) {
    // Attempt to report to the caller that they have screwed up.
    // Should this fault, we just tolerate it.
    IGNORE(ustore(&current->stack()->ef.r0, unsigned(result)));
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
