#include "k/reply_gate.h"

#include "k/context.h"
#include "k/object_table.h"

namespace k {

ReplyGate::ReplyGate()
  : _expected_brand(0) {}

void ReplyGate::deliver_from(Brand brand, Sender * sender) {
  // Filter out messages bearing the wrong brand.
  if (brand != _expected_brand) {
    sender->complete_send(Exception::bad_operation);
  }

  // Invalidate our object table entry, revoking all extant keys.
  if (++_expected_brand == 0) {
    object_table.invalidate(get_index());
  }

  // This type of gate refuses to block.  Either its owner is waiting to
  // receive, or you're doing something wrong and your message gets discarded.
  auto maybe_partner = _receivers.peek();
  if (maybe_partner) {
    auto it = maybe_partner.ref();
    it->unlink();
    it->owner->complete_blocked_receive(brand, sender);
  } else {
    sender->complete_send(Exception::bad_operation);
  }
}

void ReplyGate::deliver_to(Context * context) {
  context->block_in_receive(_receivers);
}

etl::data::Maybe<Key> ReplyGate::make_key(Brand) {
  return Object::make_key(_expected_brand);
}

}  // namespace k
