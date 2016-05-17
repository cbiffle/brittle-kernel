#include "a/rt/keys.h"

#include <cstdint>
#include "etl/assert.h"

namespace rt {

/*
 * Global variable tracking key register availability as a bitmask.  A 1
 * indicates an available register.  Note that the permanently null kr0 is
 * not initially available.
 */
static uint32_t available_keys = 0xFFFE;

void reserve_key(unsigned ki) {
  auto mask = 1u << ki;
  ETL_ASSERT(available_keys & mask);  // If it's already allocated, fail.
  available_keys &= ~mask;
}

AutoKey::AutoKey() : _ki{0} {
  for (unsigned i = 1; i < 16; ++i) {
    if (available_keys & (1u << i)) {
      available_keys &= ~(1u << i);
      _ki = i;
      return;
    }
  }

  ETL_ASSERT(false);  // out of key registers!
}

AutoKey::AutoKey(AutoKey && other) : _ki{other._ki} {
  other._ki = 0;
}

AutoKey::~AutoKey() {
  if (_ki != 0) available_keys |= 1u << _ki;
}

}  // namespace rt
