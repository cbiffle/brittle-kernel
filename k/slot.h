#ifndef K_SLOT_H
#define K_SLOT_H

/*
 * A Slot object represents the capability to create an object in an Object
 * Table slot.
 */

#include "k/object.h"

namespace k {

class Slot final : public Object {
public:
  Slot(Generation g) : Object{g} {}
  Kind get_kind() const override { return Kind::slot; }
};

}  // namespace k

#endif  // K_SLOT_H
