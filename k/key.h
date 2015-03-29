#ifndef K_KEY_H
#define K_KEY_H

#include <stdint.h>

#include "k/sys_result.h"

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
  static Key filled(unsigned index, uint32_t brand);

  /*
   * Static factory function for producing a null key.
   */
  static Key null();

  /*
   * Fills a key in-place with the given object table index and
   * brand.  After this returns the key will be current.
   */
  void fill(unsigned index, uint32_t brand);

  /*
   * Replaces the contents of a key with the null key.
   */
  void nullify();

  /*
   * Gets the object table index for the object referenced by this
   * key.
   */
  uint32_t get_index() const { return _index; }

  /*
   * Gets the brand stored within this key.
   */
  uint32_t get_brand() const { return _brand; }

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
