#include "k/object.h"

#include "k/key.h"
#include "k/keys.h"
#include "k/reply_sender.h"

namespace k {

Object::Object(Generation g) : _generation{g} {}

Maybe<Key> Object::make_key(Brand brand) {
  return { Key::filled(this, brand) };
}

void Object::deliver_to(Context * ctx) {
}

bool Object::is_memory() const {
  return false;
}

bool Object::is_gate() const {
  return false;
}

bool Object::is_slot() const {
  return false;
}

void Object::do_badop(Message const & m, Keys & k) {
  ScopedReplySender reply{k.keys[0],
    Message::failure(Exception::bad_operation, m.d0.get_selector())};
}

}  // namespace k