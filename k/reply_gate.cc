#include "k/reply_gate.h"

#include "etl/error/check.h"

#include "k/context.h"
#include "k/object_table.h"

namespace k {

ReplyGate::ReplyGate()
  : _expected_brand(0) {}

SysResult ReplyGate::deliver_from(Brand brand, Sender * sender) {
  if (brand != _expected_brand) return SysResult::bad_key;

  if (++_expected_brand == 0) {
    object_table.invalidate(get_index());
  }
  return _receivers.take()->complete_blocked_receive(brand, sender);
}

SysResult ReplyGate::deliver_to(Brand brand, Context * context) {
  return context->block_in_receive(brand, _receivers);
}

}  // namespace k
