#include "k/key.h"

#include "etl/assert.h"

#include "k/object.h"
#include "k/object_table.h"

namespace k {

Key Key::filled(TableIndex index, Brand brand) {
  Key k;

  k._brand = brand;
  k._generation = object_table()[index].get_generation();
  k._index = index;
  return k;
}

Object * Key::get() {
  auto & o = object_table()[_index];
  if (_generation == o.get_generation()) {
    return &o;
  } else {
    *this = null();
    return &object_table()[0];
  }
}

void Key::deliver_from(Sender * sender) {
  get()->deliver_from(_brand, sender);
}

}  // namespace k
