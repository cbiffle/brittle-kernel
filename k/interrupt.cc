#include "k/interrupt.h"

#include "etl/armv7m/nvic.h"
#include "etl/armv7m/scb.h"
#include "etl/assert.h"

#include "k/reply_sender.h"
#include "k/scheduler.h"

using etl::armv7m::nvic;
using etl::armv7m::scb;
using etl::armv7m::Scb;

namespace k {

/******************************************************************************
 * InterruptBase
 */

InterruptBase::InterruptBase(uint32_t identifier)
  : _saved_brand{0},
    _target{},
    _sender_item{this},
    _priority{0},
    _identifier{identifier} {}

void InterruptBase::trigger() {
  disable_interrupt();
  _target.deliver_from(this);
  do_deferred_switch_from_irq();
}

void InterruptBase::deliver_from(Brand const & brand, Sender * sender) {
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
      do_extension(m.d0.get_selector(), brand, m, k);
      break;
  }
}

void InterruptBase::do_extension(Selector,
                                 Brand const &,
                                 Message const & m,
                                 Keys & k) {
  do_badop(m, k);
}


void InterruptBase::do_set_target(Brand const &, Message const &, Keys & k) {
  auto & reply = k.keys[0];
  auto & target = k.keys[1];

  ScopedReplySender reply_sender{reply};

  _target = target;
}

void InterruptBase::do_enable(Brand const &, Message const & m, Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

  bool clear_pending = m.d1 != 0;

  if (clear_pending) clear_pending_interrupt();
  enable_interrupt();
}

Priority InterruptBase::get_priority() const {
  return _priority;
}

void InterruptBase::on_delivery_accepted(Message & m, Keys & k) {
  m = {
    Descriptor::zero().with_selector(1),
    _identifier,
  };
  k.keys[0] = make_key(0).ref();
  for (unsigned ki = 0; ki < config::n_message_keys; ++ki) {
    k.keys[ki] = Key::null();
  }
}

void InterruptBase::on_delivery_failed(Exception, uint32_t) {
  // Our attempt to deliver has failed.  This is an indication of a
  // configuration error.  Do nothing for now.
  // TODO: should this be able to raise some sort of alert?
}

void InterruptBase::block_in_send(Brand const & brand,
                              List<BlockingSender> & list) {
  _saved_brand = brand;
  list.insert(&_sender_item);
}

void InterruptBase::on_blocked_delivery_accepted(Message & m, Brand & b, Keys & k) {
  b = _saved_brand;
  on_delivery_accepted(m, k);
}

void InterruptBase::on_blocked_delivery_failed(Exception, uint32_t) {
  // We appear to have been interrupted while blocked.
}


/******************************************************************************
 * Interrupt
 */

Interrupt::Interrupt(uint32_t irq)
  : InterruptBase{irq} {}

void Interrupt::disable_interrupt() {
  nvic.disable_irq(get_identifier());
}

void Interrupt::enable_interrupt() {
  nvic.enable_irq(get_identifier());
}

void Interrupt::clear_pending_interrupt() {
  nvic.clear_pending_irq(get_identifier());
}

}  // namespace k
