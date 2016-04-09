#include "k/mm_fault.h"

#include "etl/armv7m/exception_frame.h"

#include "etl/assert.h"

#include "k/unprivileged.h"

namespace k {

void * mm_fault(void * stack) {
  // TODO: generate fault message to supervisor.
  // For now, halt execution here.
  ETL_ASSERT(false);
}

void mm_fault_k(void * vstack) {
  auto frame = static_cast<etl::armv7m::ExceptionFrame *>(vstack);

  // In case this fault happened during the execution of a deliberately
  // unprivileged operation...
  if (mm_fault_recovery_handler) {
    // ...rewrite the faulting code's stack frame to effect a jump to its
    // handler code.
    frame->r15 = mm_fault_recovery_handler;
    mm_fault_recovery_handler = 0;
  } else {
    ETL_ASSERT(false);  // failure in kernel.
  }
}

}  // namespace k
