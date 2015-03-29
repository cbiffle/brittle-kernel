#ifndef K_OBJECT_TABLE_H
#define K_OBJECT_TABLE_H

#include <stdint.h>

#include "k/config.h"
#include "k/object.h"

namespace k {

/*
 * Tracks the location and identity of every kernel object.  Provides a place
 * to store generation info that is stable across object destruction and
 * reallocation, to allow us to amortize key revocation costs more aggressively.
 *
 * The ObjectTable is itself an Object, and is accessible to the system.
 */
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

  SysResult deliver_from(uint32_t, Sender *) override;

private:
  Entry _objects[config::n_objects];

  SysResult mint_key(uint32_t, Sender *, Message const &);
  SysResult read_key(uint32_t, Sender *, Message const &);
};

extern ObjectTable object_table;

}  // namespace k

#endif  // K_OBJECT_TABLE_H
