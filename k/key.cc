#include "k/key.h"

#include "etl/assert.h"

#include "k/object.h"
#include "k/object_table.h"

namespace k {

Key Key::filled(TableIndex index, Brand brand) {
  Key k;

  k._brand = brand;
  k._generation = object_table[index].generation;
  k._index = index;
  return k;
}

Object * Key::get() {
  auto const & te = object_table[_index];
  if (_generation == te.generation && te.ptr) {
    return te.ptr;
  } else {
    *this = null();
    return object_table[0].ptr;
  }
}

void Key::deliver_from(Sender * sender) {
  get()->deliver_from(_brand, sender);
}

}  // namespace k
