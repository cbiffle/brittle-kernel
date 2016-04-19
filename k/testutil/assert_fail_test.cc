#include "etl/assert.h"
#include <gtest/gtest.h>
#include <stdexcept>

namespace etl {

static void assertion_failed2(char const * file,
                              int line,
                              char const * function,
                              char const * expression) {
  FAIL() << "Assertion failed at " << file << ":" << line;
}

void assertion_failed(char const * file,
                      int line,
                      char const * function,
                      char const * expression) {
  assertion_failed2(file, line, function, expression);
  throw new std::logic_error("test failed");
}

}  // namespace etl
