#ifndef COMMON_SELECTORS_H
#define COMMON_SELECTORS_H

#include "common/abi_types.h"

namespace selector {

namespace context {
  static constexpr Selector
    read_register = 1,
    write_register = 2,
    read_key_register = 3,
    write_key_register = 4,
    read_region_register = 5,
    write_region_register = 6,
    make_runnable = 7,
    get_priority = 8,
    set_priority = 9,
    read_low_registers = 10,
    read_high_registers = 11,
    write_low_registers = 12,
    write_high_registers = 13;
}

namespace interrupt {
  static constexpr Selector
    set_target = 1,
    enable = 2;
}

namespace memory {
  static constexpr Selector
    inspect = 1,
    change = 2,
    split = 3,
    become = 4,
    peek = 5,
    poke = 6,
    make_child = 7;
}

namespace object_table {
  static constexpr Selector
    mint_key = 1,
    read_key = 2,
    get_kind = 3,
    invalidate = 4;
}

}  // namespace selector

#endif  // COMMON_SELECTORS_H
