#include "k/key.h"

#include "etl/assert.h"

#include "k/object.h"
#include "k/object_table.h"

namespace k {

Key Key::filled(unsigned index, uint32_t brand) {
  Key k;
  k.fill(index, brand);
  return k;
}

Key null() {
  Key k;
  k.nullify();
  return k;
}

SysResult Key::deliver_from(Sender * sender) {
  lazy_revoke();

  return object_table[_index].ptr->deliver_from(_brand, sender);
}

void Key::lazy_revoke() {
  ETL_ASSERT(_index < config::n_objects);

  auto const & te = object_table[_index];
  if (_generation[0] != te.generation[0]
      || _generation[1] != te.generation[1]
      || te.ptr == nullptr) {
    nullify();
  }
}

void Key::fill(unsigned index, uint32_t brand) {
  auto const & e = object_table[index];
  _generation[0] = e.generation[0];
  _generation[1] = e.generation[1];
  _index = index;
  _brand = brand;
}

void Key::nullify() {
  _index = 0;
  _brand = 0;
  _generation[0] = object_table[0].generation[0];
  _generation[1] = object_table[0].generation[1];
}

}  // namespace k
