#include "k/reply_gate.h"

#include "common/abi_sizes.h"

#include "k/context.h"
#include "k/object_table.h"

namespace k {

template struct ObjectSubclassChecks<ReplyGate, kabi::reply_gate_size>;

void ReplyGate::deliver_from(Brand const & brand, Sender * sender) {
  // Filter out messages bearing the wrong brand, or those that arrive before
  // we're bound.
  if (!is_bound() || brand != _body.expected_brand) {
    // Fail like a null object.
    Object::deliver_from(brand, sender);
    return;
  }

  // Invalidate our object table entry, revoking all extant keys.
  if (++_body.expected_brand == 0) {
    // Also rev the generation to extend the counter's period.
    set_generation(get_generation() + 1);
  }

  auto owner = _body.owner.ref();  // Success guaranteed by is_bound check above
  if (owner->is_awaiting_reply()) {
    _body.owner.ref()->complete_blocked_receive(brand, sender);
  }
}

void ReplyGate::deliver_to(Context * context) {
  if (context != _body.owner) {
    // Who are you, and what are you doing?
    context->complete_receive(Exception::bad_operation);
    return;
  }

  context->block_in_reply();
}

Maybe<Key> ReplyGate::make_key(Brand const &) {
  return Object::make_key(_body.expected_brand);
}

}  // namespace k
