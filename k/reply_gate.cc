#include "k/reply_gate.h"

#include "k/context.h"
#include "k/object_table.h"

namespace k {

ReplyGate::ReplyGate()
  : _expected_brand(0) {}

void ReplyGate::deliver_from(Brand brand, Sender * sender) {
  if (brand != _expected_brand) {
    sender->complete_send(Exception::bad_operation);
  }

  if (++_expected_brand == 0) {
    object_table.invalidate(get_index());
  }

  _receivers.take()->complete_blocked_receive(brand, sender);
}

void ReplyGate::deliver_to(Context * context) {
  context->block_in_receive(_receivers);
}

etl::data::Maybe<Key> ReplyGate::make_key(Brand) {
  return Object::make_key(_expected_brand);
}

}  // namespace k
