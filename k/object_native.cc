#include "k/object.h"

#include "k/key.h"

namespace k {

Object::Object() : _index{0} {}

etl::data::Maybe<Key> Object::make_key(Brand brand) {
  return { Key::filled(_index, brand) };
}

void Object::deliver_to(Context * ctx) {
}

bool Object::is_address_range() const {
  return false;
}

}  // namespace k
