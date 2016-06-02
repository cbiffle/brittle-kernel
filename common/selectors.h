#ifndef COMMON_SELECTORS_H
#define COMMON_SELECTORS_H

#include "common/abi_types.h"

namespace selector {

namespace context {
  static constexpr Selector
    read_register = 0,
    write_register = 1,
    read_key_register = 2,
    write_key_register = 3,
    read_region_register = 4,
    write_region_register = 5,
    make_runnable = 6,
    get_priority = 7,
    set_priority = 8,
    read_low_registers = 9,
    read_high_registers = 10,
    write_low_registers = 11,
    write_high_registers = 12;
}

namespace interrupt {
  static constexpr Selector
    set_target = 1,
    enable = 2;
}

namespace memory {
  static constexpr Selector
    inspect = 0,
    change = 1,
    split = 2,
    become = 3,
    peek = 4,
    poke = 5,
    make_child = 6;
}

namespace object_table {
  static constexpr Selector
    mint_key = 0,
    read_key = 1,
    get_kind = 2,
    invalidate = 3;
}

}  // namespace selector

#endif  // COMMON_SELECTORS_H
