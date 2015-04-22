#include "k/interrupt.h"

#include "etl/armv7m/nvic.h"

#include "k/reply_sender.h"
#include "k/scheduler.h"

using etl::armv7m::nvic;

namespace k {

Interrupt::Interrupt(unsigned irq)
  : _saved_brand{0},
    _target{},
    _sender_item{this},
    _priority{0},
    _irq{irq} {}

void Interrupt::trigger() {
  nvic.disable_irq(_irq);
  _target.deliver_from(this);
  do_deferred_switch_from_irq();
}

void Interrupt::deliver_from(Brand brand, Sender * sender) {
  Message m = sender->get_message();
  switch (m.d0.get_selector()) {
    case 1:
      do_set_target(brand, sender, m);
      break;

    case 2:
      do_enable(brand, sender, m);
      break;

    default:
      // TODO: this seems wrong; should be sent through reply key.
      sender->complete_send(Exception::bad_operation, m.d0.get_selector());
      break;
  }
}

void Interrupt::do_set_target(Brand brand, Sender * sender, Message const & m) {
  auto reply = sender->get_message_key(0);
  auto target = sender->get_message_key(1);
  sender->complete_send();

  _target = target;

  ReplySender reply_sender{0};  // TODO
  reply.deliver_from(&reply_sender);
}

void Interrupt::do_enable(Brand brand, Sender * sender, Message const & m) {
  auto reply = sender->get_message_key(0);
  bool clear_pending = m.d1 != 0;
  sender->complete_send();

  if (clear_pending) nvic.clear_pending_irq(_irq);
  nvic.enable_irq(_irq);

  ReplySender reply_sender{0};  // TODO
  reply.deliver_from(&reply_sender);
}

Priority Interrupt::get_priority() const {
  return _priority;
}

Message Interrupt::get_message() {
  return {
    Descriptor::zero().with_selector(1),
    _irq,
  };
}

Key Interrupt::get_message_key(unsigned index) {
  if (index == 0) {
    // Pass a key that can be used to re-enable interrupts as the "reply key."
    return {make_key(0).ref()};
  } else {
    return Key::null();
  }
}

Brand Interrupt::get_saved_brand() const {
  return _saved_brand;
}

void Interrupt::complete_send() {
  // We've successfully delivered to a task without blocking.  Our interrupt is
  // still disabled.  Now we wait for the task to "reply" and re-enable us.
}

void Interrupt::complete_send(Exception, uint32_t) {
  // Our attempt to deliver has failed.  This is an indication of a
  // configuration error.
}

void Interrupt::block_in_send(Brand brand, List<BlockingSender> & list) {
  _saved_brand = brand;
  list.insert(&_sender_item);
}

void Interrupt::complete_blocked_send() {
  // We were blocked, but now we're not.  Merely being unblocked isn't enough
  // to re-enable the interrupt; so no action is needed here.
}

void Interrupt::complete_blocked_send(Exception, uint32_t) {
  // We appear to have been interrupted while blocked.
}

}  // namespace k
