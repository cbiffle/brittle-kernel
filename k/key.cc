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
  lazy_revoke();
  return object_table[_index].ptr;
}

void Key::deliver_from(Sender * sender) {
  get()->deliver_from(_brand, sender);
}

void Key::lazy_revoke() {
  ETL_ASSERT(_index < config::n_objects);

  auto const & te = object_table[_index];
  if (_generation != te.generation || te.ptr == nullptr) {
    *this = null();
  }
}

}  // namespace k
