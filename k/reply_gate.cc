#include "k/reply_gate.h"

#include "common/abi_sizes.h"

#include "k/context.h"
#include "k/object_table.h"

namespace k {

template struct ObjectSubclassChecks<ReplyGate, kabi::reply_gate_size>;

void ReplyGate::deliver_from(Brand const & brand, Sender * sender) {
  // Filter out messages bearing the wrong brand.
  if (brand != _body.expected_brand) {
    // Fail like a null object.
    sender->on_delivery_failed(Exception::bad_operation);
    return;
  }

  // Invalidate our object table entry, revoking all extant keys.
  if (++_body.expected_brand == 0) {
    // Also rev the generation to extend the counter's period.
    set_generation(get_generation() + 1);
  }

  // This type of gate refuses to block.  Either its owner is waiting to
  // receive, or you're doing something wrong and your message gets discarded.
  if (auto partner = _body.receivers.take()) {
    partner.ref()->complete_blocked_receive(brand, sender);
  } else {
    sender->on_delivery_failed(Exception::bad_operation);
  }
}

void ReplyGate::deliver_to(Context * context) {
  if (context == _body.owner) {
    context->block_in_receive(_body.receivers);
  } else {
    // Who are you, and what are you doing?
    context->complete_receive(Exception::bad_operation);
  }
}

Maybe<Key> ReplyGate::make_key(Brand) {
  return Object::make_key(_body.expected_brand);
}

}  // namespace k
