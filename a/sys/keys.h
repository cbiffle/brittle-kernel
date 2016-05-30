#ifndef SYS_KEYS_H
#define SYS_KEYS_H

#include "a/sys/types.h"

namespace sys {

namespace ki {
  static constexpr KeyIndex
    null = 0,
    ot = 1,
    self = 2;
  // REMEMBER TO UPDATE KEYS.CC

}  // namespace ki

}  // namespace sys

#endif  // SYS_KEYS_H
