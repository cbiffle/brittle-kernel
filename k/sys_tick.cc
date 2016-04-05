#include "k/sys_tick.h"

#include "etl/armv7m/scb.h"
#include "etl/armv7m/sys_tick.h"

#include "k/reply_sender.h"

using etl::armv7m::scb;
using etl::armv7m::Scb;
using etl::armv7m::sys_tick;

namespace k {

SysTick::SysTick()
  : InterruptBase{0} {}

void SysTick::disable_interrupt() {
  sys_tick.write_csr(sys_tick.read_csr().with_tickint(false));
}

void SysTick::enable_interrupt() {
  sys_tick.write_csr(sys_tick.read_csr().with_tickint(true));
}

void SysTick::clear_pending_interrupt() {
  scb.write_icsr(Scb::icsr_value_t{}.with_pendstclr(true));
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
      reply_sender.set_message(Message::failure(Exception::index_out_of_range));
      break;
  }

#undef STREG

  if (ok) {
    reply_sender.set_message({Descriptor::zero(), value});
  }
}

void SysTick::do_write_register(Brand const &, Message const & m, Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

#define STREG(num, name) case num: sys_tick.write_ ## name(m.d2); break;

  switch (m.d1) {
    STREG(0, csr)
    STREG(1, rvr)
    STREG(2, cvr)
    STREG(3, calib)

    default:
      reply_sender.set_message(Message::failure(Exception::index_out_of_range));
      break;
  }

#undef STREG
}

}  // namespace k
