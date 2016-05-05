#include "k/key.h"

#include "k/object.h"
#include "k/object_table.h"
#include "k/panic.h"

namespace k {

Key Key::filled(Object * ptr, Brand const & brand) {
  PANIC_UNLESS(ptr, "nullptr in Key::filled");

  Key k;

  k._brand = brand;
  k._generation = ptr->get_generation();
  k._ptr = ptr;
  return k;
}

Key Key::null() {
  Key k;

  k._brand = 0;
  k._generation = 0;
  k._ptr = &object_table()[0];
  return k;
}

Object * Key::get() {
  if (!_ptr || _generation != _ptr->get_generation()) {
    *this = null();
  }
  return _ptr;
}

void Key::deliver_from(Sender * sender) {
  get()->deliver_from(_brand, sender);
}

}  // namespace k
