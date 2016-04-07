#ifndef K_SYS_TICK_H
#define K_SYS_TICK_H

#include "k/interrupt.h"

namespace k {

class SysTick final : public InterruptBase {
public:
  SysTick();

private:
  void disable_interrupt() override;
  void clear_pending_interrupt() override;
  void enable_interrupt() override;
  void do_extension(Selector, Brand const &, Message const &, Keys &) override;

  void do_set_target(Brand const &, Message const &, Keys &);
  void do_enable(Brand const &, Message const &, Keys &);
  void do_read_register(Brand const &, Message const &, Keys &);
  void do_write_register(Brand const &, Message const &, Keys &);
};

}  // namespace k

#endif  // K_SYS_TICK_H
