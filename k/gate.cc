#include "k/gate.h"

#include "k/context.h"

namespace k {

void Gate::deliver_from(Brand brand, Sender * sender) {
  if (_receivers.is_empty()) {
    sender->block_in_send(brand, _senders);
  } else {
    _receivers.take()->complete_blocked_receive(brand, sender);
  }
}

void Gate::deliver_to(Context * receiver) {
  if (_senders.is_empty()) {
    receiver->block_in_receive(_receivers);
  } else {
    receiver->complete_receive(_senders.take());
  }
}

}  // namespace k
