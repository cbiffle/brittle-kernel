#ifndef K_KEYS_H
#define K_KEYS_H

#include "k/key.h"
#include "k/config.h"

namespace k {

/*
 * Holder for one message's worth of keys.
 */
struct Keys {
  Key keys[config::n_message_keys];
};

}  // namespace k

#endif  // K_KEYS_H
