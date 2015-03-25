#include "etl/armv7m/exception_frame.h"

#include "etl/assert.h"

#include "k/unprivileged.h"

namespace k {

void * mm_fault(void * stack) {
  return stack;
}

void mm_fault_k(void * vstack) {
  auto frame = static_cast<etl::armv7m::ExceptionFrame *>(vstack);

  if (mm_fault_recovery_handler) {
    frame->r15 = mm_fault_recovery_handler;
    mm_fault_recovery_handler = 0;
  } else {
    ETL_ASSERT(false);  // failure in kernel.
  }
}

}  // namespace k
