#include "k/gate.h"

#include "common/abi_sizes.h"
#include "common/exceptions.h"
#include "common/selectors.h"

#include "k/context.h"
#include "k/keys.h"
#include "k/reply_sender.h"

namespace k {

template struct ObjectSubclassChecks<Gate, kabi::gate_size>;

static constexpr Brand transparent_mask = Brand(1) << 63;

void Gate::deliver_from(Brand const & brand, Sender * sender) {
  if (brand & transparent_mask) {
    if (auto partner = _body.receivers.take()) {
      partner.ref()->complete_blocked_receive(brand, sender);
    } else {
      sender->block_in_send(brand, _body.senders);
    }
    return;
  }

  // Handle the message locally per the Gate protocol.
  Keys k;
  auto m = sender->on_delivery(k);
  ScopedReplySender reply_sender{k.keys[0]};

  namespace S = selector::gate;
  switch (m.desc.get_selector()) {
    case S::make_client_key:
      reply_sender.set_key(1,
          make_key(m.d0 | (Brand(m.d1) << 32) | transparent_mask).ref());
      return;

    default:
      reply_sender.message() =
        Message::failure(Exception::bad_operation, m.desc.get_selector());
      return;
  }
}

void Gate::deliver_to(Brand const & brand, Context * receiver) {
  if (brand & transparent_mask) {
    // Reject attempts to receive through a transparent (client) key.
    receiver->complete_receive(Exception::bad_operation);
    return;
  }

  if (auto partner = _body.senders.take()) {
    receiver->complete_receive(partner.ref());
  } else {
    receiver->block_in_receive(_body.receivers);
  }
}

}  // namespace k
