#include "k/irq_entry.h"

#include "etl/armv7m/exception_table.h"
#include "etl/armv7m/instructions.h"
#include "etl/armv7m/registers.h"

#include "k/interrupt.h"
#include "k/irq_redirector.h"
#include "k/scheduler.h"
#include "k/sys_tick.h"

namespace k {

void irq_redirector() {
  etl::armv7m::disable_interrupts();

  auto exception_number = etl::armv7m::get_ipsr() & 0x1FF;

  InterruptBase * handler;
  if (exception_number >= 16) {
    // External interrupt.
    handler = get_irq_redirection_table()[exception_number - 16];
  } else if (exception_number == 15) {
    // SysTick.
    handler = get_sys_tick_redirector();
  } else {
    // Some other fault, errantly routed here.
    ETL_ASSERT(false);
  }

  ETL_ASSERT(handler);
  handler->trigger();
  do_deferred_switch_from_irq();

  etl::armv7m::enable_interrupts();
}

}  // namespace k

void etl_armv7m_sys_tick_handler()
  __attribute__((alias("_ZN1k14irq_redirectorEv")));
