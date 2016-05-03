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
 * Pointer-like reference to some Keys.
 */
class KeysRef {
public:
  constexpr KeysRef(RangePtr<Key> storage)
    : _storage{storage} {}

  constexpr Key & operator[](unsigned index) const {
    return _storage[index];
  }

private:
  RangePtr<Key> _storage;
};

}  // namespace k

#endif  // K_KEYS_H
