#include "k/object.h"

#include "k/key.h"

namespace k {

Object::Object() : _index{0} {}

Key Object::make_key(uint32_t brand) {
  return Key::filled(_index, brand);
}

}  // namespace k
