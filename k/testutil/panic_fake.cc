#include "k/panic.h"
#include <gtest/gtest.h>
#include <stdexcept>

namespace k {

// This is broken out because Googletest is weird, and FAIL contains a return.
// panic() is noreturn, and can't use FAIL.  Yay.
static void panic2(char const * message) {
  FAIL() << "PANIC: " << message;
}

void panic(char const * message) {
  panic2(message);
  throw new std::logic_error("test failed");
}

}  // namespace k
