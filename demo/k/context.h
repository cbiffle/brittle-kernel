#ifndef DEMO_CONTEXT_H
#define DEMO_CONTEXT_H

#include <cstdint>

namespace context {

bool set_region(unsigned k, unsigned region_index, unsigned region_key);
bool get_region(unsigned k, unsigned region_index, unsigned region_key_out);

enum class Register : uint32_t {
  r4, r5, r6, r7, r8, r9, r10, r11,
  basepri,
  sp,
};

bool set_register(unsigned k, Register, uint32_t value);

bool set_key(unsigned k, unsigned key_index, unsigned key);

bool make_runnable(unsigned k);

bool set_priority(unsigned k, unsigned priority);

}  // namespace context

#endif  // DEMO_CONTEXT_H
