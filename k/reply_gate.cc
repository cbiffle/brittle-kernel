#include "k/reply_gate.h"

#include "etl/error/check.h"

#include "k/context.h"
#include "k/object_table.h"

namespace k {

SysResult ReplyGate::deliver_from(uint32_t brand, Sender * sender) {
  object_table.invalidate(get_index());
  return _receivers.take()->complete_blocked_receive(brand, sender);
}

SysResult ReplyGate::deliver_to(uint32_t brand, Context * context) {
  return context->block_in_receive(brand, _receivers);
}

}  // namespace k
