#include "k/sender.h"

#include "etl/assert.h"

#include "k/key.h"

namespace k {

Key Sender::get_message_key(unsigned index) {
  return Key::null();
}

void Sender::complete_send(SysResult) {
}

SysResult Sender::block_in_send(Brand, List<Sender> &) {
  return SysResult::would_block;
}

void Sender::complete_blocked_send() {
  ETL_ASSERT(false);
}

}  // namespace k
