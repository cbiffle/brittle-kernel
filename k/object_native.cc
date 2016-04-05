#include "k/object.h"

#include "k/key.h"
#include "k/keys.h"
#include "k/reply_sender.h"

namespace k {

Object::Object() : _index{0} {}

Maybe<Key> Object::make_key(Brand brand) {
  return { Key::filled(_index, brand) };
}

void Object::deliver_to(Context * ctx) {
}

bool Object::is_address_range() const {
  return false;
}

bool Object::is_gate() const {
  return false;
}

void Object::do_badop(Message const & m, Keys & k) {
  ScopedReplySender reply{k.keys[0],
    Message::failure(Exception::bad_operation, m.d0.get_selector())};
}

}  // namespace k
