#ifndef COMMON_ABI_SIZES_H
#define COMMON_ABI_SIZES_H

#include "k/config.h"  // TODO move information to common

namespace kabi {

static constexpr unsigned
  address_range_size = 0,
  context_size = 512,
  gate_size = k::config::n_priorities * 16,
  interrupt_size = 32 + k::config::n_priorities * 8,
  reply_gate_size = 8 + k::config::n_priorities * 8,
  sys_tick_size = interrupt_size;


}  // namespace kabi

#endif  // COMMON_ABI_SIZES_H
