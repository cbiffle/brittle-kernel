#ifndef DEMO_INTERRUPT_H
#define DEMO_INTERRUPT_H

#include <cstdint>

namespace interrupt {

bool set_target(unsigned k, unsigned target_key);
bool enable(unsigned k, bool clear_pending = false);

}  // namespace interrupt

#endif  // DEMO_INTERRUPT_H
