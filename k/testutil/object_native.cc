#include "k/object.h"

#include "k/key.h"
#include "k/keys.h"
#include "k/reply_sender.h"
#include "k/sender.h"

namespace k {

Object::Object(Generation g) : _generation{g} {}

Maybe<Key> Object::make_key(Brand const & brand) {
  return { Key::filled(this, brand) };
}

void Object::deliver_from(Brand const &, Sender * sender) {
  Keys k;
  auto m = sender->on_delivery(k);
  do_badop(m, k);
}

void Object::deliver_to(Context * ctx) {
}

Region Object::get_region_for_brand(Brand const &) const {
  return { Region::Rbar{}, Region::Rasr{} };
}

void Object::do_badop(Message const & m, Keys & k) {
  ScopedReplySender reply{k.keys[0],
    Message::failure(Exception::bad_operation, m.desc.get_selector())};
}

}  // namespace k
