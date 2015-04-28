#include "k/object.h"

#include "k/key.h"
#include "k/keys.h"
#include "k/reply_sender.h"

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

void Object::do_badop(Message const & m, Keys & k) {
  ReplySender reply{Message::failure(Exception::bad_operation,
                                     m.d0.get_selector())};
  k.keys[0].deliver_from(&reply);
}

}  // namespace k
