#include "k/reply_gate.h"

#include "k/context.h"
#include "k/object_table.h"

namespace k {

ReplyGate::ReplyGate()
  : _expected_brand{0} {}

void ReplyGate::deliver_from(Brand const & brand, Sender * sender) {
  // Filter out messages bearing the wrong brand.
  if (brand != _expected_brand) {
    // Fail like a null object.
    sender->on_delivery_failed(Exception::bad_operation);
    return;
  }

  // Invalidate our object table entry, revoking all extant keys.
  if (++_expected_brand == 0) {
    // Also rev the generation to extend the counter's period.
    object_table().invalidate(get_index());
  }

  // This type of gate refuses to block.  Either its owner is waiting to
  // receive, or you're doing something wrong and your message gets discarded.
  if (auto partner = _receivers.take()) {
    partner.ref()->complete_blocked_receive(brand, sender);
  } else {
    sender->on_delivery_failed(Exception::bad_operation);
  }
}

void ReplyGate::deliver_to(Context * context) {
  context->block_in_receive(_receivers);
}

Maybe<Key> ReplyGate::make_key(Brand) {
  return Object::make_key(_expected_brand);
}

}  // namespace k
