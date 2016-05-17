#ifndef A_K_OBJECT_TABLE_H
#define A_K_OBJECT_TABLE_H

#include <cstdint>

#include "a/rt/keys.h"

namespace object_table {

struct KeyInfo {
  uint32_t index;
  uint64_t brand;
};

KeyInfo read_key(unsigned k, unsigned key);

enum class Kind : uint32_t {
  null,
  object_table,
  slot,
  memory,
  context,
  gate,
  interrupt,
};

Kind get_kind(unsigned k, unsigned index);

rt::AutoKey mint_key(unsigned k, unsigned index, uint64_t brand);

bool invalidate(unsigned k, unsigned index, bool rollover_ok = false);

}  // namespace object_table

#endif  // A_K_OBJECT_TABLE_H
