#include "k/key.h"

#include "etl/assert.h"

#include "k/object.h"
#include "k/object_table.h"

namespace k {

Key Key::filled(TableIndex index, Brand brand) {
  Key k;
  k.fill(index, brand);
  return k;
}

Key Key::null() {
  Key k;
  k.nullify();
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
    nullify();
  }
}

void Key::fill(TableIndex index, Brand brand) {
  auto const & e = object_table[index];
  _brand = brand;
  _generation = e.generation;
  _index = index;
}

void Key::nullify() {
  _brand = 0;
  _generation = object_table[0].generation;
  _index = 0;
}

}  // namespace k
