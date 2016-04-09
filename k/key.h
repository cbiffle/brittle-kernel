#ifndef K_KEY_H
#define K_KEY_H

#include "common/abi_types.h"

namespace k {

struct Object;  // see: k/object.h
struct Sender;  // see: k/sender.h

/*
 * A Key is the data structure used to reference kernel objects.
 */
class Key {
public:
  /*
   * Static factory function for producing a key filled in with the
   * given object table index and brand.
   */
  static Key filled(TableIndex index, Brand brand);

  /*
   * Static factory function for producing a null key.
   */
  static constexpr Key null() { return {}; }

  /*
   * Gets the object table index for the object referenced by this
   * key.
   */
  TableIndex get_index() const { return _index; }

  /*
   * Gets the brand stored within this key.
   */
  Brand get_brand() const { return _brand; }

  Object * get();

  /*
   * Facade function for Object::deliver_from; calls through to the
   * referenced object, supplemented by this key's brand.
   */
  void deliver_from(Sender *);

private:
  // Uninterpreted data, invisible to the key holder, but  made available to
  // the object when the key is used.
  Brand _brand;
  // Distinguishes successive occupants of a single object table slot.
  Generation _generation;
  // Index of the object table slot.
  TableIndex _index;

  void lazy_revoke();
};

}  // namespace k

#endif  // K_KEY_H
