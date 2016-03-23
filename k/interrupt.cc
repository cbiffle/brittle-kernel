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

void Interrupt::deliver_from(Brand const & brand, Sender * sender) {
  Message m;
  Keys k;
  sender->on_delivery_accepted(m, k);
  switch (m.d0.get_selector()) {
    case 1:
      do_set_target(brand, m, k);
      break;

    case 2:
      do_enable(brand, m, k);
      break;

    default:
      do_badop(m, k);
      break;
  }
}

void Interrupt::do_set_target(Brand const &, Message const &, Keys & k) {
  auto & reply = k.keys[0];
  auto & target = k.keys[1];

  _target = target;

  ReplySender reply_sender;
  reply.deliver_from(&reply_sender);
}

void Interrupt::do_enable(Brand const &, Message const & m, Keys & k) {
  bool clear_pending = m.d1 != 0;

  if (clear_pending) nvic.clear_pending_irq(_irq);
  nvic.enable_irq(_irq);

  ReplySender reply_sender;
  k.keys[0].deliver_from(&reply_sender);
}

Priority Interrupt::get_priority() const {
  return _priority;
}

void Interrupt::on_delivery_accepted(Message & m, Keys & k) {
  m = {
    Descriptor::zero().with_selector(1),
    _irq,
  };
  k.keys[0] = make_key(0).ref();
  for (unsigned ki = 0; ki < config::n_message_keys; ++ki) {
    k.keys[ki] = Key::null();
  }
}

void Interrupt::on_delivery_failed(Exception, uint32_t) {
  // Our attempt to deliver has failed.  This is an indication of a
  // configuration error.  Do nothing for now.
  // TODO: should this be able to raise some sort of alert?
}

void Interrupt::block_in_send(Brand const & brand,
                              List<BlockingSender> & list) {
  _saved_brand = brand;
  list.insert(&_sender_item);
}

void Interrupt::on_blocked_delivery_accepted(Message & m, Brand & b, Keys & k) {
  b = _saved_brand;
  on_delivery_accepted(m, k);
}

void Interrupt::on_blocked_delivery_failed(Exception, uint32_t) {
  // We appear to have been interrupted while blocked.
}

}  // namespace k
