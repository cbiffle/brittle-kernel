#ifndef K_KEYS_H
#define K_KEYS_H

#include "k/key.h"
#include "k/config.h"
#include "k/range_ptr.h"

namespace k {

/*
 * Holder for one message's worth of keys.
 */
struct Keys {
  Key keys[config::n_message_keys];
};

/*
 * Pointer-like reference to some Keys, which can be arbitrarily shuffled by
 * a keymap.
 */
class KeysRef {
public:
  /*
   * Constructor for a fixed-size bank of four keys, commonly used within the
   * kernel with a default map.  The map indices are small enough to avoid
   * overrun.
   */
  constexpr KeysRef(Keys & keys)
    : _storage{keys.keys},
      _map{0x3210} {}

  /*
   * Constructor for a larger array of Keys, presumed to have at least 16
   * entries and ordered using an explicit map.
   */
  constexpr explicit KeysRef(Key keys[], uint32_t map)
    : _storage{keys},
      _map{map} {}

  constexpr unsigned map(unsigned index) const {
    return (_map >> (4 * index)) & 0xF;
  }

  constexpr Key const & get(unsigned index) const {
    return _storage[map(index)];
  }

  void set(unsigned index, Key const & k) const {
    _storage[map(index)] = k;
  }

private:
  Key * _storage;
  uint32_t _map;
};

}  // namespace k

#endif  // K_KEYS_H
