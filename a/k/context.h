#ifndef A_K_CONTEXT_H
#define A_K_CONTEXT_H

#include <cstdint>

#include "a/rt/keys.h"

namespace context {

void set_region(unsigned k, unsigned region_index, unsigned region_key);
rt::AutoKey get_region(unsigned k, unsigned region_index);

enum class Register : uint32_t {
  r4, r5, r6, r7, r8, r9, r10, r11,
  basepri,
  sp,
};

void set_register(unsigned k, Register, uint32_t value);

void set_key(unsigned k, unsigned key_index, unsigned key);

void make_runnable(unsigned k);

void set_priority(unsigned k, unsigned priority);

}  // namespace context

#endif  // A_K_CONTEXT_H
