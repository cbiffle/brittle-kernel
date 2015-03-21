#include <assert.h>
#include <stdio.h>

#include "stubs.h"

/*******************************************************************************
 * Assorted declarations.
 */

static constexpr bool trace = false;

// Names for clist slots
static constexpr uintptr_t
  // Where reply keys arrive.
  k0 = 0,
  // We move reply keys here immediately so they don't get stomped
  // by subsequent operations.
  k_saved_reply = 4,
  // We accept one flush request and keep its reply key pending
  // here across concurrent operations on other ports.
  k_flush_reply = 5,
  // Register access grant.
  k_reg = 14,
  // System access.
  k_sys = 15;

// Names for message types
static constexpr uintptr_t
  // mon protocol
  t_mon_heartbeat = 0,

  // mem protocol
  t_mem_write32 = 0,
  t_mem_read32 = 1,
  
  // uart.tx protocol
  t_tx_send1 = 0,
  t_tx_flush = 1,
  
  // uart.rx protocol
  t_rx_recv1 = 0;

// Names for our ports.
static constexpr uintptr_t
  p_mon = 0,
  p_tx = 1,
  p_irq = 2,
  p_rx = 3;

// Utility function for using k_saved_reply with no keys.
static void reply(uintptr_t md0 = 0,
                  uintptr_t md1 = 0,
                  uintptr_t md2 = 0,
                  uintptr_t md3 = 0) {
  send(false, k_saved_reply, Message{{md0, md1, md2, md3}});
}


/*******************************************************************************
 * Mem protocol wrappers for UART register access
 */

static void write_cr1(uint32_t value) {
  send(true, k_reg, Message{t_mem_write32, 0xC, value});
}

static void write_dr(uint8_t value) {
  send(true, k_reg, Message{t_mem_write32, 4, value});
}

static uint32_t read_dr() {
  ReceivedMessage response{};
  auto r = call(true, k_reg, Message{t_mem_read32, 4}, &response);
  assert(r == SysResult::success);
  return uint32_t(response.m.data[0]);
}

static uint32_t read_cr1() {
  ReceivedMessage response{};
  auto r = call(true, k_reg, Message{t_mem_read32, 0xC}, &response);
  assert(r == SysResult::success);
  return uint32_t(response.m.data[0]);
}

static uint32_t read_sr() {
  ReceivedMessage response{};
  auto r = call(true, k_reg, Message{t_mem_read32, 0}, &response);
  assert(r == SysResult::success);
  return uint32_t(response.m.data[0]);
}

static void enable_irq_on_txe() {
  write_cr1(read_cr1() | (1 << 7));
}

static void enable_irq_on_rxne() {
  write_cr1(read_cr1() | (1 << 5));
}

static void enable_irq_on_tc() {
  write_cr1(read_cr1() | (1 << 6));
}


/*******************************************************************************
 * Mon server implementation
 */
static void handle_mon(Message const & req) {
  switch (req.data[0]) {
    case t_mon_heartbeat:
      reply(req.data[1], req.data[2], req.data[3]);
      break;
  }
}

/*******************************************************************************
 * UART.TX server implementation
 */
static bool flushing;

static void do_send1(Message const & req) {
  write_dr(uint8_t(req.data[1]));
  mask(p_tx);
  enable_irq_on_txe();
  reply();
}

static void do_flush(Message const &) {
  enable_irq_on_tc();
  mask(p_tx);
  move_cap(k_saved_reply, k_flush_reply);
  flushing = true;
  // Do not reply yet.
}

static void handle_tx(Message const & req) {
  switch (req.data[0]) {
    case t_tx_send1: do_send1(req); break;
    case t_tx_flush: do_flush(req); break;
  }
}

/*******************************************************************************
 * IRQ server implementation
 */
static void handle_irq(Message const &) {
  auto sr = read_sr();
  auto cr1 = read_cr1();

  if ((sr & (1 << 7)) && (cr1 & (1 << 7))) {
    // TXE set and interrupt enabled.
    // Disable interrupt in preparation for allowing another send.
    cr1 &= ~(1 << 7);
    write_cr1(cr1);
    if (!flushing) unmask(p_tx);
  }

  if ((sr & (1 << 5)) && (cr1 & (1 << 5))) {
    // RxNE set and interrupt enabled.
    cr1 &= ~(1 << 5);
    write_cr1(cr1);
    unmask(p_rx);
  }

  if ((sr & (1 << 6)) && (cr1 & (1 << 6))) {
    // TC set and interrupt enabled -- a flush has completed.
    cr1 &= ~(1 << 6);
    write_cr1(cr1);
    assert(flushing);
    send(false, k_flush_reply, Message{{0, 0, 0, 0}});
    flushing = false;
    unmask(p_tx);
  }

  reply(1);
}

/*******************************************************************************
 * UART.RX server implementation
 */
static void do_recv1(Message const & req) {
  auto b = read_dr();
  mask(p_rx);
  enable_irq_on_rxne();
  reply(b);
}

static void handle_rx(Message const & req) {
  switch (req.data[0]) {
    case t_rx_recv1: do_recv1(req); break;
  }
}

/*******************************************************************************
 * Main loop
 */
int main() {
  mask(p_rx);

  if (trace) fprintf(stderr, "CHILD: starting message loop\n");
  while (true) {
    if (trace) fprintf(stderr, "CHILD: entering open receive\n");
    ReceivedMessage rm {};
    auto r = open_receive(true, &rm);
    if (r != SysResult::success) continue;

    move_cap(k0, k_saved_reply);
    // TODO: clear transients

    switch (rm.port) {
      case p_mon: handle_mon(rm.m); break;
      case p_tx: handle_tx(rm.m); break;
      case p_irq: handle_irq(rm.m); break;
      case p_rx: handle_rx(rm.m); break;
    }
  }
}
