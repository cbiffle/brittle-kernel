#ifndef K_OBJECT_TABLE_H
#define K_OBJECT_TABLE_H

/*
 * The Object Table tracks the location and identity of every kernel Object.
 * It is, itself, an Object accessible to the system, implementing the
 * Object Table protocol (what else?).
 *
 *
 * A Study in Alternatives
 * -----------------------
 *
 * The Object Table is fixed size, which imposes a maximum object count, so you
 * might be wondering why it exists.
 *
 * In short: dangling pointers and efficient key revocation.
 *
 * Imagine we want to deallocate an object at address 42.  But it's a popular
 * object, with keys stored in all sorts of places.  If the keys contain
 * direct pointers, we might try maintaining a list of keys for each object and
 * nulling them out on revocation.  But that's O(n) in the popularity of the
 * object -- a load-dependent time cost, which the kernel avoids.
 *
 * Instead of directly deallocating the object, we could replace it with some
 * sort of "tombstone" object and leave the pointers in place.  Anyone trying
 * to use a key to the dead object would notice the tombstone (say, by its
 * virtual method behavior) and revoke the keys lazily as they are discovered.
 * This can work, up until the memory is reused for a new object -- now any
 * keys that haven't been lazily revoked have changed into keys to a new
 * object!  Making this correct would require a sort of scavenging or garbage
 * collection process, which is a hard sell in a hard-real-time application.
 *
 * We could add a "generation" counter to objects.  Whenever a new object is
 * allocated in memory previously allocated to an old object, the generation
 * gets incremented.  We'd then extend the key to contain both a pointer, and a
 * generation.  Whenever a key is used, we'd check its generation against the
 * object's, and lazily revoke any keys that fail to match.
 *
 * However, unless we require all objects to be the same size, the memory
 * freed by the death of object 42 may be merged with adjacent memory to
 * allocate a larger object.  Now the old keys point *inside* a larger object,
 * somewhere arbitrary!  In that case, we can't even *find* the generation
 * field to read it and discover our error.
 *
 * To ensure that we can find the generation field, we can move it out of the
 * (variably-sized, variably-aligned) objects and into a central location.  A
 * table.  To find the table entry associated with a particular key, we could
 * a table index in the key instead of a pointer, and put a pointer to the real
 * object in the table next to the generation.  This was the original version of
 * the Object Table.
 *
 * To allow for "flyweight" objects that do not require memory to create and
 * destroy -- specifically, Slot and Memory -- we later moved slightly more
 * ojbect data into the table: the vtable pointer, and a word of object-specific
 * data.
 *
 * In exchange for a limit (application defined) on the number of living objects
 * and the cost of some indirections, the Object Table provides:
 *
 *  1. O(1) key revocation, by incrementing the Generation field.
 *
 *  2. Protection against key resurrection for 2^32 generations.
 *
 *  3. Relatively small (128-bit) key representation.
 *
 *
 * Historical Note
 * ---------------
 *
 * It may amuse you to note that, through convergent evolution, the Object Table
 * closely resembles a similar structure in the Intel iAPX 432 processor,
 * though the implementation details are quite different.
 */

#include <cstdint>

#include "common/abi_types.h"

#include "k/object.h"
#include "k/range_ptr.h"

namespace k {

class ObjectTable final : public Object {
public:
  ObjectTable(Generation);

  struct alignas(Object) Entry {
    uint8_t bytes[Object::max_head_size];

    Object & as_object() {
      return *reinterpret_cast<Object *>(this);
    }
  };

  /*
   * Used during initialization to give the ObjectTable its actual table
   * memory.  This can be called exactly once.
   * 
   * Precondition: has not yet been called.
   */
  void set_entries(RangePtr<Entry>);

  /*
   * Looks up an Object by index.
   */
  Object & operator[](TableIndex index) {
    return _objects[index].as_object();
  }

  // Implementation of Object.
  Kind get_kind() const override { return Kind::object_table; }
  void deliver_from(Brand const &, Sender *) override;

private:
  RangePtr<Entry> _objects;

  void do_mint_key(Brand const &, Message const &, Keys &);
  void do_read_key(Brand const &, Message const &, Keys &);
  void do_get_kind(Brand const &, Message const &, Keys &);
  void do_invalidate(Brand const &, Message const &, Keys &);
};

/*
 * Access to singleton instance, provided during initialization.
 */
ObjectTable & object_table();
void set_object_table(ObjectTable *);
void reset_object_table_for_test();

}  // namespace k

#endif  // K_OBJECT_TABLE_H
