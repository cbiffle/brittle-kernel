#include "k/reply_sender.h"

#include "etl/assert.h"

#include "k/object.h"

namespace k {

ReplySender::ReplySender()
  : _m{},
    _keys{{}} {}

ReplySender::ReplySender(Message const & m)
  : _m{m},
    _keys{{}} {}

void ReplySender::set_key(unsigned index, Key const & k) {
  ETL_ASSERT(index < config::n_message_keys);
  _keys.keys[index] = k;
}

Message ReplySender::on_delivery_accepted(Keys & keys) {
  keys = _keys;
  return _m;
}

void ReplySender::on_delivery_failed(Exception, uint32_t) {
  // Don't care.
}

void ReplySender::block_in_send(Brand, List<BlockingSender> &) {
  // No.
}


ScopedReplySender::~ScopedReplySender() {
  auto obj = k.get();
  if (obj->get_kind() == Object::Kind::null) return;
  obj->deliver_from(k.get_brand(), &rs);
}

}  // namespace k
