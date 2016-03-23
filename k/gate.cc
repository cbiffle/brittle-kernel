#include "k/gate.h"

#include "k/context.h"

namespace k {

void Gate::deliver_from(Brand const & brand, Sender * sender) {
  if (auto partner = _receivers.take()) {
    partner.ref()->complete_blocked_receive(brand, sender);
  } else {
    sender->block_in_send(brand, _senders);
  }
}

void Gate::deliver_to(Context * receiver) {
  if (auto partner = _senders.take()) {
    receiver->complete_receive(partner.ref());
  } else {
    receiver->block_in_receive(_receivers);
  }
}

}  // namespace k
