#include "k/irq_entry.h"

#include "etl/armv7m/exception_table.h"
#include "etl/armv7m/instructions.h"
#include "etl/armv7m/registers.h"

#include "k/interrupt.h"
#include "k/irq_redirector.h"
#include "k/panic.h"
#include "k/scheduler.h"

namespace k {

void irq_redirector() {
  etl::armv7m::disable_interrupts();

  auto exception_number = etl::armv7m::get_ipsr() & 0x1FF;

  Interrupt * handler;
  if (exception_number >= 15) {
    // External interrupt.
    handler = get_irq_redirection_table()[exception_number - 15];
  } else {
    PANIC("fault sent to IRQ redirector");
  }

  PANIC_UNLESS(handler, "null IRQ redirector entry");
  handler->trigger();
  do_deferred_switch_from_irq();

  etl::armv7m::enable_interrupts();
}

}  // namespace k

void etl_armv7m_sys_tick_handler()
  __attribute__((alias("_ZN1k14irq_redirectorEv")));
