#ifndef K_KEY_H
#define K_KEY_H

#include <stdint.h>

#include "k/sys_result.h"

namespace k {

struct Message;  // see: k/ipc.h

/*
 * A Key is the data structure used to reference kernel objects.
 */
class Key {
public:
  SysResult call(Message const *, Message *);

  void fill(unsigned index, uint32_t brand);
  void nullify();

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
