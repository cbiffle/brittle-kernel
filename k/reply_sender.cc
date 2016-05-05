#include "k/reply_sender.h"

#include "etl/array_count.h"

#include "k/object.h"
#include "k/panic.h"

namespace k {

ReplySender::ReplySender()
  : _m{},
    _keys{{}} {}

ReplySender::ReplySender(Message const & m)
  : _m{m},
    _keys{{}} {}

void ReplySender::set_key(unsigned index, Key const & k) {
  PANIC_UNLESS(index < config::n_message_keys, "bad key index in ReplySender");
  _keys.keys[index] = k;
}

Message ReplySender::on_delivery(KeysRef keys) {
  for (unsigned i = 0; i < etl::array_count(_keys.keys); ++i) {
    keys.set(i, _keys.keys[i]);
  }
  return _m;
}

void ReplySender::block_in_send(Brand const &, List<BlockingSender> &) {
  // No.
}


ScopedReplySender::~ScopedReplySender() {
  auto obj = k.get();
  if (obj->get_kind() == Object::Kind::null) return;
  obj->deliver_from(k.get_brand(), &rs);
}

}  // namespace k
