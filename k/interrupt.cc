#include "k/interrupt.h"

#include "etl/armv7m/nvic.h"
#include "etl/armv7m/scb.h"
#include "etl/armv7m/sys_tick.h"

#include "common/abi_sizes.h"

#include "k/irq_redirector.h"
#include "k/reply_sender.h"

using etl::armv7m::nvic;
using etl::armv7m::Scb;
using etl::armv7m::scb;
using etl::armv7m::sys_tick;

namespace k {

template struct ObjectSubclassChecks<Interrupt, kabi::interrupt_size>;

static constexpr uint32_t sys_tick_identifier = ~uint32_t(0);

Interrupt::Interrupt(Generation g, Body & body)
  : Object{g}, _body(body) {
  _body.sender_item.owner = this;
  get_irq_redirection_table()[get_identifier() + 1] = this;
}

void Interrupt::trigger() {
  disable_interrupt();
  if (_body.sender_item.is_linked()) {
    // We took an interrupt while we're still blocking to deliver a previous
    // one!  TODO: what shall we do in this case?  Well, "not crash" is a great
    // start.
    return;
  }
  _body.target.deliver_from(this);
}

void Interrupt::deliver_from(Brand brand, Sender * sender) {
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
      do_badop(m, k);
      break;
  }
}

void Interrupt::do_set_target(Brand, Message const &, Keys & k) {
  auto & reply = k.keys[0];
  auto & target = k.keys[1];

  ScopedReplySender reply_sender{reply};

  _body.target = target;
}

void Interrupt::do_enable(Brand, Message const & m, Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

  bool clear_pending = m.d1 != 0;

  if (clear_pending) clear_pending_interrupt();
  enable_interrupt();
}

Priority Interrupt::get_priority() const {
  return _body.priority;
}

Message Interrupt::on_delivery_accepted(Keys & k) {
  k.keys[0] = make_key(0).ref();
  for (unsigned ki = 1; ki < config::n_message_keys; ++ki) {
    k.keys[ki] = Key::null();
  }
  return {
    Descriptor::zero().with_selector(1),
    _body.identifier,
  };
}

void Interrupt::block_in_send(Brand brand,
                              List<BlockingSender> & list) {
  _body.saved_brand = brand;
  list.insert(&_body.sender_item);
}

ReceivedMessage Interrupt::on_blocked_delivery_accepted(Keys & k) {
  return {
    .m = on_delivery_accepted(k),
    .brand = _body.saved_brand,
  };
}

void Interrupt::on_blocked_delivery_aborted() {
  // We appear to have been interrupted while blocked.
  // TODO: do we need to do something?
}

void Interrupt::disable_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  auto id = get_identifier();
  if (id == sys_tick_identifier) {
    // SysTick
    sys_tick.write_csr(sys_tick.read_csr().with_tickint(false));
  } else {
    // Boring old interrupt.
    nvic.disable_irq(get_identifier());
  }
#endif
}

void Interrupt::enable_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  auto id = get_identifier();
  if (id == sys_tick_identifier) {
    // SysTick
    sys_tick.write_csr(sys_tick.read_csr().with_tickint(true));
  } else {
    // Boring old interrupt.
    nvic.enable_irq(get_identifier());
  }
#endif
}

void Interrupt::clear_pending_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  auto id = get_identifier();
  if (id == sys_tick_identifier) {
    // SysTick
    scb.write_icsr(Scb::icsr_value_t{}.with_pendstclr(true));
  } else {
    nvic.clear_pending_irq(get_identifier());
  }
#endif
}

}  // namespace k
