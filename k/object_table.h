#ifndef K_OBJECT_TABLE_H
#define K_OBJECT_TABLE_H

#include <stdint.h>

#include "k/config.h"

namespace k {

class Object;  // see: k/object.h

struct ObjectTableEntry {
  // Distinguishes successive occupants of this entry.  Must match the
  // generation field of a referencing key for the key to be considered
  // valid.
  uint32_t generation[2];
  // Address of the object occupying this slot, or nullptr if the slot is
  // available.
  Object * ptr;
};

extern ObjectTableEntry objects[config::n_objects];

}  // namespace k

#endif  // K_OBJECT_TABLE_H
