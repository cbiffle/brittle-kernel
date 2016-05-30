#ifndef A_SYS_LOAD_H
#define A_SYS_LOAD_H

#include "a/maybe.h"
#include "a/rt/keys.h"
#include "a/sys/types.h"

namespace sys {

Maybe<rt::AutoKey> load_program(uintptr_t img_addr, KeyIndex img_key);

}  // namespace sys

#endif  // A_SYS_LOAD_H
