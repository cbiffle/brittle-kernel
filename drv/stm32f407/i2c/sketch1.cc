Message m_irq;
Message m_transact;

uint32_t wait_for_irq() {
  if (m_irq.brand) reply_to(m_irq);
  m_irq = receive();
  return m_irq.brand;
}

void do_transact() {
  mask(C_USER);

  set_alarm(m_transact.deadline);

  // Clear accumulated errors.
  hw.write_sr1(0);

  // Start a transaction.
  hw.write_cr1(hw.read_cr1().with_start(true));

  switch (wait_for_irq()) {
    case MB_EVENT_IRQ:
      auto sr1 = hw.read_sr1();
      if (sr1.get_sb()) break;

      // TODO: what would other conditions even mean?
      return;

    case MB_ERROR_IRQ:
    case MB_ALARM:
      // TODO: it is not clear how to abort in this case.
      return;
  }


}
