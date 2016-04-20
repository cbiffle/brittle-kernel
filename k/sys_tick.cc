#include "k/sys_tick.h"

#include "etl/armv7m/scb.h"
#include "etl/armv7m/sys_tick.h"

#include "common/abi_sizes.h"

#include "k/irq_redirector.h"
#include "k/reply_sender.h"

using etl::armv7m::scb;
using etl::armv7m::Scb;
using etl::armv7m::sys_tick;

namespace k {

template struct ObjectSubclassChecks<SysTick, kabi::sys_tick_size>;

SysTick::SysTick(Generation g, Body & body)
  : InterruptBase{g, body} {
  set_sys_tick_redirector(this);
}

void SysTick::disable_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  sys_tick.write_csr(sys_tick.read_csr().with_tickint(false));
#endif
}

void SysTick::enable_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  sys_tick.write_csr(sys_tick.read_csr().with_tickint(true));
#endif
}

void SysTick::clear_pending_interrupt() {
#ifndef HOSTED_KERNEL_BUILD
  scb.write_icsr(Scb::icsr_value_t{}.with_pendstclr(true));
#endif
}

void SysTick::do_extension(Selector s,
                           Brand const & brand,
                           Message const & m,
                           Keys & k) {
  switch (s) {
    case 3:
      do_read_register(brand, m, k);
      break;

    case 4:
      do_write_register(brand, m, k);
      break;

    default:
      do_badop(m, k);
      break;
  }
}

void SysTick::do_read_register(Brand const &, Message const & m, Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

  uint32_t value;
  bool ok = true;

#define STREG(num, name) case num: value = uint32_t(sys_tick.read_ ## name()); break;

  switch (m.d1) {
    STREG(0, csr)
    STREG(1, rvr)
    STREG(2, cvr)
    STREG(3, calib)

    default:
      ok = false;
      reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
      break;
  }

#undef STREG

  if (ok) {
    reply_sender.get_message().d1 = value;
  }
}

void SysTick::do_write_register(Brand const &, Message const & m, Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

#define STREG(num, name) case num: sys_tick.write_ ## name(m.d2); break;

#ifndef HOSTED_KERNEL_BUILD
  switch (m.d1) {
    STREG(0, csr)
    STREG(1, rvr)
    STREG(2, cvr)
    STREG(3, calib)

    default:
      reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
      break;
  }
#endif

#undef STREG
}

}  // namespace k
