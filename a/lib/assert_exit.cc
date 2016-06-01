#include "etl/assert.h"

#include "a/sys/api.h"

namespace etl {

void assertion_failed(char const * file,
                      int line,
                      char const *,
                      char const *) {
  sys::exit(15,  // TODO centralize system key register index
      sys::ExitReason::assert_failed,
      reinterpret_cast<uint32_t>(file),
      line);
}


}  // namespace etl
