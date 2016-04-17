#include "k/key.h"

#include "etl/assert.h"

#include "k/object.h"
#include "k/object_table.h"

namespace k {

Key Key::filled(TableIndex index, Brand brand) {
  Key k;

  k._brand = brand;
  auto p = object_table()[index].ptr;
  k._generation = p ? p->get_generation() : 0;
  k._index = index;
  return k;
}

Object * Key::get() {
  auto const & te = object_table()[_index];
  if (te.ptr && _generation == te.ptr->get_generation()) {
    return te.ptr;
  } else {
    *this = null();
    return object_table()[0].ptr;
  }
}

void Key::deliver_from(Sender * sender) {
  get()->deliver_from(_brand, sender);
}

}  // namespace k
