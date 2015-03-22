#include "k/svc.h"

#include "etl/prediction.h"

#include "etl/armv7m/registers.h"

#include "k/context.h"

namespace k {

static bool booted = false;

void svc_dispatch() {
  if (ETL_LIKELY(booted)) {
    // Normal invocation.
  } else {
    // First syscall to start initial task.
    etl::armv7m::set_control(1);
    etl::armv7m::set_psp(k::current->stack);
    booted = true;
  }
}

}  // namespace k
