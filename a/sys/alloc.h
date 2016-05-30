#ifndef A_SYS_ALLOC_H
#define A_SYS_ALLOC_H

#include "a/sys/types.h"
#include "a/maybe.h"
#include "a/rt/keys.h"

namespace sys {

bool free_mem(KeyIndex);
Maybe<rt::AutoKey> alloc_mem(unsigned l2_half_size, uint64_t brand);

}  // namespace sys

#endif  // A_SYS_ALLOC_H
