#ifndef K_NULL_OBJECT_H
#define K_NULL_OBJECT_H

#include "k/object.h"

namespace k {

/*
 * The Null Object is the initial object referenced by all keys, before
 * they have been initialized from some other key.  It is also the object
 * to which keys refer after revocation.
 *
 * It does nothing -- just generates delivery exceptions.
 */
class NullObject final : public Object {
public:
  NullObject(Generation);
  Kind get_kind() const override { return Kind::null; }
};

static_assert(sizeof(NullObject) <= 16,
    "Null Object too big");

}  // namespace k

#endif  // K_NULL_OBJECT_H
