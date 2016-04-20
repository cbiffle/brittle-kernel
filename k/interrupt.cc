#include "k/interrupt.h"

#include "etl/armv7m/nvic.h"
#include "etl/assert.h"

#include "common/abi_sizes.h"

#include "k/reply_sender.h"

using etl::armv7m::nvic;

namespace k {

/******************************************************************************
 * InterruptBase
 */

void InterruptBase::trigger() {
  disable_interrupt();
  if (_body.sender_item.is_linked()) {
    // We took an interrupt while we're still blocking to deliver a previous
    // one!  TODO: what shall we do in this case?  Well, "not crash" is a great
    // start.
    return;
  }
  _body.target.deliver_from(this);
}

void InterruptBase::deliver_from(Brand const & brand, Sender * sender) {
  Keys k;
  Message m = sender->on_delivery_accepted(k);
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

  _body.target = target;
}

void InterruptBase::do_enable(Brand const &, Message const & m, Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

  bool clear_pending = m.d1 != 0;

  if (clear_pending) clear_pending_interrupt();
  enable_interrupt();
}

Priority InterruptBase::get_priority() const {
  return _body.priority;
}

Message InterruptBase::on_delivery_accepted(Keys & k) {
  k.keys[0] = make_key(0).ref();
  for (unsigned ki = 1; ki < config::n_message_keys; ++ki) {
    k.keys[ki] = Key::null();
  }
  return {
    Descriptor::zero().with_selector(1),
    _body.identifier,
  };
}

void InterruptBase::on_delivery_failed(Exception, uint32_t) {
  // Our attempt to deliver has failed.  This is an indication of a
  // configuration error.  Do nothing for now.
  // TODO: should this be able to raise some sort of alert?
}

void InterruptBase::block_in_send(Brand const & brand,
                              List<BlockingSender> & list) {
  _body.saved_brand = brand;
  list.insert(&_body.sender_item);
}

Message InterruptBase::on_blocked_delivery_accepted(Brand & b, Keys & k) {
  b = _body.saved_brand;
  return on_delivery_accepted(k);
}

void InterruptBase::on_blocked_delivery_failed(Exception, uint32_t) {
  // We appear to have been interrupted while blocked.
}


/******************************************************************************
 * Interrupt
 */

template struct ObjectSubclassChecks<Interrupt, kabi::interrupt_size>;

void Interrupt::disable_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  nvic.disable_irq(get_identifier());
#endif
}

void Interrupt::enable_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  nvic.enable_irq(get_identifier());
#endif
}

void Interrupt::clear_pending_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  nvic.clear_pending_irq(get_identifier());
#endif
}

}  // namespace k
