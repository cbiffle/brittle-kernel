#ifndef K_OBJECT_TABLE_H
#define K_OBJECT_TABLE_H

#include "common/abi_types.h"

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
class ObjectTable final : public Object {
public:
  struct Entry {
    // Distinguishes successive occupants of this entry.  Must match the
    // generation field of a referencing key for the key to be considered
    // valid.
    Generation generation;
    // Address of the object occupying this slot, or nullptr if the slot is
    // available.
    Object * ptr;
  };

  Entry & operator[](TableIndex index) { return _objects[index]; }

  /*
   * Invalidates all extant keys for the object at a given index.  The keys
   * will be lazily revoked.
   *
   * Precondition: index is valid.
   */
  void invalidate(TableIndex index);

  void deliver_from(Brand const &, Sender *) override;

private:
  Entry _objects[config::n_objects];

  void do_mint_key(Brand const &, Message const &, Keys &);
  void do_read_key(Brand const &, Message const &, Keys &);
};

extern ObjectTable object_table;

}  // namespace k

#endif  // K_OBJECT_TABLE_H
