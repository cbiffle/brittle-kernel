
/*
 * USART driver task.
 *
 */
int task_main() {
  while (true) {
    Message m = receive();

    switch (m.chan) {
      case C_IRQ: handle_irq(m); break;
      case C_TX:  handle_tx(m); break;
      case C_RX:  handle_rx(m); break;
      case C_MON: handle_mon(m); break;
    }
  }
}

void handle_tx(Message const &m) {
  switch (m.type) {
    case MT_TX_SEND1: do_send1(m); break;
    case MT_TX_FLUSH: do_flush(m); break;
  }
}

void do_send1(Message const &m) {
  txq.push(m.arg0);
  if (!txq.is_empty()) hw.write_cr1(hw.read_cr1().with_txeie(true));
  if (txq.is_full()) mask(m.chan);
  reply(m);
}

void do_flush(Message const &m) {
  flushing = true;
  flush_msg = m;
  hw.write_cr1(hw.read_cr1().with_tcie(true));
  mask(C_TX);
}

void handle_rx(Message const &m) {
  switch (m.type) {
    case MT_RX_RECEIVE1: do_receive1(m); break;
  }
}

void do_receive1(Message const &m) {
  uint8_t b = rxq.pop();
  if (rxq.is_empty()) mask(m.chan);
  if (!rxq.is_full()) hw.write_cr1(hw.read_cr1().with_rxneie(true));
  reply(m, b);
}

void handle_irq(Message const & m) {
  auto status = hw.read_status();
  auto cr1 = hw.read_cr1();

  if (status.get_tc() && cr1.get_tcie()) {
    if (txq.is_empty()) {
      reply(flush_msg);
      flushing = false;
      unmask(C_TX);
    } else {
      // If something else in the system delays our ability to
      // handle this interrupt, we could easily observe TC before
      // the queue is emptied.  Ignore this.  We'll refeed the
      // pipeline below.
    }
  }

  if (status.get_txe() && cr1.get_txeie()) {
    hw.write_data(txq.pop());
    if (txq.is_empty()) {
      hw.write_cr1(cr1.with_txeie(false));
    }

    if (!flushing) unmask(C_TX);
  }

  if (status.get_rxne() && cr1.get_rxneie()) {
    uint16_t d = hw.read_data();
    if (status.get_ore()) d |= RX_ERROR_OVERRUN;
    if (status.get_nf()) d |= RX_ERROR_NOISE;
    if (status.get_fe()) d |= RX_ERROR_FRAMING;
    if (status.get_pe()) d |= RX_ERROR_PARITY;

    rxq.push(d);

    if (rxq.is_full()) {
      hw.write_cr1(cr1.with_rxneie(false));
    }
    unmask(C_RX);
  }

  reply(m);
}
