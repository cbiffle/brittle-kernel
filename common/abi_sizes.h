#ifndef COMMON_ABI_SIZES_H
#define COMMON_ABI_SIZES_H

#include "k/config.h"  // TODO move information to common

namespace kabi {

static constexpr unsigned
  memory_size = 0,
  context_size = 512,
  gate_size = k::config::n_priorities * 16,
  interrupt_size = 32 + k::config::n_priorities * 8,
  reply_gate_size = 8 + k::config::n_priorities * 8;

constexpr unsigned log2floor(unsigned x) {
  return (x < 2) ? 0
                 : 1 + log2floor(x >> 1);
}

constexpr unsigned log2ceil(unsigned x) {
  return (x == 1) ? 0 : log2floor(x - 1) + 1;
}

constexpr unsigned allocsize(unsigned x) {
  return log2ceil(x) < 5 ? 5 : log2ceil(x);
}

static constexpr unsigned
  memory_l2_size = allocsize(memory_size),
  context_l2_size = allocsize(context_size),
  gate_l2_size = allocsize(gate_size),
  interrupt_l2_size = allocsize(interrupt_size),
  reply_gate_l2_size = allocsize(reply_gate_size);

}  // namespace kabi

#endif  // COMMON_ABI_SIZES_H
