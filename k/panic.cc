#include "k/panic.h"

#include "etl/assert.h"

namespace k {

__attribute__((naked))
void panic(char const * message) {
  asm volatile (
      "cpsid i \n"
      "b .");
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
