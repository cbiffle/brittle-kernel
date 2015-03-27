#ifndef K_KEY_H
#define K_KEY_H

#include <stdint.h>

#include "k/sys_result.h"

namespace k {

struct Context;  // see: k/context.h

/*
 * A Key is the data structure used to reference kernel objects.
 */
class Key {
public:
  SysResult call(Context * caller);

  void fill(unsigned index, uint32_t brand);
  void nullify();

  uint32_t get_index() const { return _index; }
  uint32_t get_brand() const { return _brand; }

private:
  // Distinguishes successive occupants of a single object table slot.
  uint32_t _generation[2];
  // Index of the object table slot.
  uint32_t _index;
  // 32 bits of uninterpreted data made available to the object when the
  // key is used.
  uint32_t _brand;

  void lazy_revoke();
};

}  // namespace k

#endif  // K_KEY_H
