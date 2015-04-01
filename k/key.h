#ifndef K_KEY_H
#define K_KEY_H

#include "k/sys_result.h"
#include "k/types.h"

namespace k {

struct Context;  // see: k/context.h
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
  static Key null();

  /*
   * Fills a key in-place with the given object table index and
   * brand.  After this returns the key will be current.
   */
  void fill(TableIndex index, Brand brand);

  /*
   * Replaces the contents of a key with the null key.
   */
  void nullify();

  /*
   * Gets the object table index for the object referenced by this
   * key.
   */
  TableIndex get_index() const { return _index; }

  /*
   * Gets the brand stored within this key.
   */
  Brand get_brand() const { return _brand; }

  /*
   * Facade function for Object::deliver_from; calls through to the
   * referenced object, supplemented by this key's brand.
   */
  SysResult deliver_from(Sender *);

  /*
   * Facade function for Object::deliver_to; calls through to the
   * referenced object, supplemented by this key's brand.
   */
  SysResult deliver_to(Context *);

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
