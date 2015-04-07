#include "k/reply_sender.h"

namespace k {

ReplySender::ReplySender(uint32_t priority)
  : _m{},
    _keys{},
    _priority{priority} {}

ReplySender::ReplySender(uint32_t priority, Message const & m)
  : _m{m},
    _keys{},
    _priority{priority} {}

void ReplySender::set_key(unsigned index, Key const & k) {
  ETL_ASSERT(index < config::n_message_keys);
  _keys[index] = k;
}

uint32_t ReplySender::get_priority() const { return _priority; }

Message ReplySender::get_message() {
  return _m;
}

Key ReplySender::get_message_key(unsigned index) {
  ETL_ASSERT(index < config::n_message_keys);
  return _keys[index];
}

void ReplySender::complete_send() {
  // Don't care.
}

void ReplySender::complete_send(Exception, uint32_t) {
  // Don't care about this either.
}

void ReplySender::block_in_send(Brand, List<Sender> &) {
  // No.
}

}  // namespace k
