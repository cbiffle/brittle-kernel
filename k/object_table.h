#ifndef K_OBJECT_TABLE_H
#define K_OBJECT_TABLE_H

#include <stdint.h>

#include "k/config.h"
#include "k/object.h"

namespace k {

class ObjectTable : public Object {
public:
  struct Entry {
    // Distinguishes successive occupants of this entry.  Must match the
    // generation field of a referencing key for the key to be considered
    // valid.
    uint32_t generation[2];
    // Address of the object occupying this slot, or nullptr if the slot is
    // available.
    Object * ptr;
  };

  Entry & operator[](unsigned index) { return _objects[index]; }

  SysResult call(uint32_t, Message const *, Message *) override;

private:
  Entry _objects[config::n_objects];
};

extern ObjectTable object_table;

}  // namespace k

#endif  // K_OBJECT_TABLE_H