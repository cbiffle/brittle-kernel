#ifndef A_K_INTERRUPT_H
#define A_K_INTERRUPT_H

#include <cstdint>

namespace interrupt {

void set_target(unsigned k, unsigned target_key);
void enable(unsigned k, bool clear_pending = false);

}  // namespace interrupt

#endif  // A_K_INTERRUPT_H
