#include "k/panic.h"

#include "etl/assert.h"

namespace k {

__attribute__((naked))
void panic(char const * message) {
  // Mark 'message' as an input, to ensure it remains in registers after LTO.
  // (Otherwise debugging gets harder.)
  asm volatile (
      "cpsid i \n"
      "b ."
      :: "r"(message));
}

}  // namespace k

namespace etl {
  void assertion_failed(char const * file,
                        int line,
                        char const * function,
                        char const * expression) {
    // Parameters are named to help GDB display them.
    PANIC("assert failure in ETL");
  }
}
