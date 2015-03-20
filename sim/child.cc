#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "protocol.h"

static constexpr bool trace = false;

/*******************************************************************************
 * Runtime
 */

template <typename T>
static void out(T const & data) {
  if (trace) fprintf(stderr, "writing %zu bytes\n", sizeof(data));
  if (write(1, &data, sizeof(data)) != sizeof(data)) {
    fprintf(stderr, "CHILD: write failed\n");
    exit(1);
  }
}

template <typename T>
static T in() {
  T tmp;
  if (trace) fprintf(stderr, "trying to read %zu bytes\n", sizeof(tmp));
  if (read(0, &tmp, sizeof(tmp)) != sizeof(tmp)) {
    fprintf(stderr, "CHILD: read failed\n");
    exit(1);
  }
  return tmp;
}

static SysResult send(bool block, uintptr_t target, Message const & m) {
  out(RequestType::send);
  out(SendRequest {
      .blocking = block,
      .target = target,
      .m = m,
  });

  auto rt = in<ResponseType>();
  assert(rt == ResponseType::complete);
  auto r = in<CompleteResponse>();
  return r.result;
}

static SysResult open_receive(bool block, ReceivedMessage * rm) {
  out(RequestType::open_receive);
  out(OpenReceiveRequest { .blocking = block });

  switch (in<ResponseType>()) {
    case ResponseType::message:
      {
        auto r = in<MessageResponse>();
        *rm = r.rm;
        return SysResult::success;
      }

    case ResponseType::complete:
      {
        auto r = in<CompleteResponse>();
        assert(r.result != SysResult::success);
        return r.result;
      }

    default:
      assert(false);
  }
}

static SysResult call(bool block,
                      uintptr_t target,
                      Message const & m,
                      ReceivedMessage * rm) {
  out(RequestType::call);
  out(SendRequest {
      .blocking = block,
      .target = target,
      .m = m,
  });

  switch (in<ResponseType>()) {
    case ResponseType::complete:
      {
        auto r = in<CompleteResponse>();
        assert(r.result != SysResult::success);
        return r.result;
      }

    case ResponseType::message:
      {
        auto r = in<MessageResponse>();
        *rm = r.rm;
        return SysResult::success;
      }

    default:
      assert(false);
  }
}


/*******************************************************************************
 * Under Test
 */

// Names for clist slots
static constexpr uintptr_t
  k0 = 0,
  k_saved_reply = 4,
  k_flush_reply = 5,
  k_reg = 14,
  k_sys = 15;

// Names for message types
static constexpr uintptr_t
  // sys protocol
  t_sys_move_cap = 0,
  t_sys_mask = 1,
  t_sys_unmask = 2,

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
// TODO: currently kind of mixed up with brands.
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

// sys protocol
static void move_cap(uintptr_t from, uintptr_t to) {
  send(true, k_sys, Message{t_sys_move_cap, from, to});
}

static void mask(uintptr_t port_key) {
  send(true, k_sys, Message{t_sys_mask, port_key});
}

static void unmask(uintptr_t port_key) {
  send(true, k_sys, Message{t_sys_unmask, port_key});
}


// mem protocol wrappers for UART register access
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
  if (trace) fprintf(stderr, "CHILD: starting message loop\n");
  while (true) {
    if (trace) fprintf(stderr, "CHILD: entering open receive\n");
    ReceivedMessage rm {};
    auto r = open_receive(true, &rm);
    if (r != SysResult::success) continue;

    move_cap(k0, k_saved_reply);
    // TODO: clear transients

    switch (rm.brand) {
      case p_mon: handle_mon(rm.m); break;
      case p_tx: handle_tx(rm.m); break;
      case p_irq: handle_irq(rm.m); break;
      case p_rx: handle_rx(rm.m); break;
    }
  }
}
