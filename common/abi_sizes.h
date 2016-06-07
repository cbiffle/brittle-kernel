#ifndef COMMON_ABI_SIZES_H
#define COMMON_ABI_SIZES_H

#include "k/config.h"  // TODO move information to common

namespace kabi {

/*
 * Number of objects that are always created by the kernel boot process.
 * These occupy the first slots in the Object Table.
 */
static constexpr unsigned well_known_object_count = 3;

/*
 * Object table indices of well-known objects.
 */
static constexpr unsigned
  oi_null = 0,
  oi_object_table = 1,
  oi_first_context = 2;

/*
 * Size of kernel objects and Object Table entries, in bytes.
 */
static constexpr unsigned
  object_head_size = 32,  // object table entry size
  context_size = 448,
  gate_size = k::config::n_priorities * 16,
  interrupt_size = 48;

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

/*
 * Size of kernel objects, expressed as ceil(log2(size)).
 */
static constexpr unsigned
  context_l2_size = allocsize(context_size),
  gate_l2_size = allocsize(gate_size),
  interrupt_l2_size = allocsize(interrupt_size);

}  // namespace kabi

#endif  // COMMON_ABI_SIZES_H
