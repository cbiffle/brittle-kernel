#include "k/sender.h"

#include "etl/assert.h"

#include "k/key.h"

namespace k {

Key Sender::get_message_key(unsigned index) {
  return Key::null();
}

void Sender::complete_blocked_send() {
  ETL_ASSERT(false);
}

void Sender::complete_blocked_send(Exception, uint32_t) {
  ETL_ASSERT(false);
}

}  // namespace k
