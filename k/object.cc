#include "k/object.h"

#include "k/key.h"

namespace k {

Object::Object() : _index{0} {}

etl::data::Maybe<Key> Object::make_key(Brand brand) {
  return { Key::filled(_index, brand) };
}

SysResult Object::deliver_to(Brand, Context *) {
  return SysResult::bad_key;
}

}  // namespace k
