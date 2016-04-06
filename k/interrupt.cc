#include "k/interrupt.h"

#include "etl/armv7m/nvic.h"
#include "etl/armv7m/scb.h"
#include "etl/armv7m/sys_tick.h"
#include "etl/assert.h"

#include "k/reply_sender.h"
#include "k/scheduler.h"

using etl::armv7m::nvic;
using etl::armv7m::scb;
using etl::armv7m::Scb;
using etl::armv7m::sys_tick;

namespace k {

Interrupt::Interrupt(int32_t irq)
  : _saved_brand{0},
    _target{},
    _sender_item{this},
    _priority{0},
    _irq{irq} {
  // We only allow a single "negative interrupt", namely -1, which means the
  // SysTick.
  ETL_ASSERT(irq >= -1);
}

void Interrupt::trigger() {
  // Disable the interrupt.
  if (_irq < 0) {
    // SysTick redirection.
    sys_tick.write_csr(sys_tick.read_csr().with_tickint(false));
  } else {
    // External interrupt.
    nvic.disable_irq(_irq);
  }

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
  ReplySender reply_sender;

  bool clear_pending = m.d1 != 0;

  if (_irq < 0) {
    if (clear_pending) scb.write_icsr(Scb::icsr_value_t{}.with_pendsvclr(true));
    sys_tick.write_csr(sys_tick.read_csr().with_tickint(true));
  } else {
    if (clear_pending) nvic.clear_pending_irq(_irq);
    nvic.enable_irq(_irq);
  }

  k.keys[0].deliver_from(&reply_sender);
}

Priority Interrupt::get_priority() const {
  return _priority;
}

void Interrupt::on_delivery_accepted(Message & m, Keys & k) {
  m = {
    Descriptor::zero().with_selector(1),
    uint32_t(_irq),
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
