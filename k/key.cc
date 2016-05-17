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
  return object_table()[0].make_key(0).ref();
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
