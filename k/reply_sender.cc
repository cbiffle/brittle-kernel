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

SysResultWith<Message> ReplySender::get_message() {
  return {etl::error::right, _m};
}

Key ReplySender::get_message_key(unsigned index) {
  ETL_ASSERT(index < config::n_message_keys);
  return _keys[index];
}

}  // namespace k
