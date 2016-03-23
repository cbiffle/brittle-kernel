#include "k/reply_sender.h"

#include "etl/assert.h"

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

void ReplySender::on_delivery_accepted(Message & m, Keys & keys) {
  m = _m;
  keys = _keys;
}

void ReplySender::on_delivery_failed(Exception, uint32_t) {
  // Don't care.
}

void ReplySender::block_in_send(Brand const &, List<BlockingSender> &) {
  // No.
}

}  // namespace k
