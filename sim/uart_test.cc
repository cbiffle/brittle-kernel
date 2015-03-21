#include <gtest/gtest.h>

#include "protocol.h"
#include "mock.h"

class UartTest : public ::sim::MockTest {
protected:
  UartTest() : MockTest("latest/sim/uart") {}

  static constexpr uintptr_t
      p_mon = 0,
      p_tx = 1,
      p_irq = 2,
      p_rx = 3,

      k0 = 0,
      k_saved_reply = 8;

  void SetUp() override {
    MockTest::SetUp();

    get_task().set_key(14, "hw");
    get_task().set_key(15, "<SYS>");
  }

  sim::SendBuilder expect_reply_to(char const * name) {
    return expect_send_to(name, Blocking::no);
  }

  void expect_write32(char const * name, uintptr_t offset, uint32_t value) {
    expect_send_to(name)
      .with_data(0, offset, value)
      .and_succeed();
  }

  void expect_read32(char const * name, uintptr_t offset, uint32_t value) {
    expect_call_to(name)
      .with_data(1, offset)
      .and_provide(0, {value}, {});
  }
};

TEST_F(UartTest, Heartbeat) {
  expect_open_receive()
      .and_provide(p_mon, {0, 1, 2, 3}, {"reply@hb"});

  expect_reply_to("reply@hb")
      .with_data(1, 2, 3)
      .and_succeed();
}

TEST_F(UartTest, SendByte) {
  expect_open_receive()
    .and_provide(p_tx, {0, 0x42}, {"reply@send"});

  // Write data register.
  expect_write32("hw", 0x4, 0x42);
  expect_read32 ("hw", 0xC, 0xDEAD0000);
  expect_write32("hw", 0xC, 0xDEAD0000 | (1 << 7));

  // Final reply.
  expect_reply_to("reply@send").and_succeed();
  
  ASSERT_TRUE(get_task().is_port_masked(p_tx));

  /*
   * Interrupt when that byte hits the wire.
   */
  expect_open_receive()
    .and_provide(p_irq, {}, {"reply@irq"});

  // Status register read.
  expect_read32("hw", 0x0, 0xBEEF0000 | (1 << 7));
  // Control register read.
  expect_read32("hw", 0xC, 0xF00D0000 | (1 << 7));

  // Control register write to squash interrupt.
  expect_write32("hw", 0xC, 0xF00D0000);

  expect_reply_to("reply@irq").with_data(1).and_succeed();

  ASSERT_FALSE(get_task().is_port_masked(p_tx));
  // TODO: what about masking effects on other ports we're not monitoring?
  // do we need some sort of ACL?
}

TEST_F(UartTest, ReceiveByte) {
  /*
   * Interrupt announcing receipt of a byte.
   */
  expect_open_receive()
    .and_provide(p_irq, {}, {"reply@irq"});

  ASSERT_TRUE(get_task().is_port_masked(p_rx));

  // Status register read.
  expect_read32("hw", 0x0, 0xBEEF0000 | (1 << 5));
  // Control register read.
  expect_read32("hw", 0xC, 0xF00D0000 | (1 << 5));

  // Control register write to squash interrupt.
  expect_write32("hw", 0xC, 0xF00D0000);

  expect_reply_to("reply@irq").with_data(1).and_succeed();

  ASSERT_FALSE(get_task().is_port_masked(p_rx));

  /*
   * Receive a byte.
   */
  expect_open_receive()
    .and_provide(p_rx, {0}, {"reply@rx"});

  // Read data register.
  expect_read32("hw", 0x4, 0x69);

  // Read, modify, write CR1 to re-enable RxNE interrupt.
  expect_read32("hw", 0xC, 0xDEAD0000);
  expect_write32("hw", 0xC, 0xDEAD0000 | (1 << 5));

  // Final reply.
  expect_reply_to("reply@rx").with_data(0x69).and_succeed();

  ASSERT_TRUE(get_task().is_port_masked(p_rx));
}

TEST_F(UartTest, FlushWhileIdle) {
  expect_open_receive()
    .and_provide(p_tx, {1}, {"reply@tx"});

  // Read, modify, write CR1 to enable TC interrupt.
  expect_read32("hw", 0xC, 0xDEAD0000);
  expect_write32("hw", 0xC, 0xDEAD0000 | (1 << 6));

  // No reply just yet.

  // Interrupt!
  expect_open_receive()
    .and_provide(p_irq, {}, {"reply@irq"});

  // TODO: really, we want this port to be masked before we get to the
  // open_receive above.  That is actually what we've achieved here,
  // since expect_open_receive will process N syscalls followed by one
  // open_receive call.  But it reads funny.
  ASSERT_TRUE(get_task().is_port_masked(p_tx));

  // Status register read.
  expect_read32("hw", 0x0, 0xBEEF0000 | (1 << 6));
  // Control register read.
  expect_read32("hw", 0xC, 0xF00D0000 | (1 << 6));
  // Control register write to squash interrupt.
  expect_write32("hw", 0xC, 0xF00D0000);

  // Flush is finished, reply to saved cap.
  expect_reply_to("reply@tx").and_succeed();

  // Reply to interrupt.
  expect_reply_to("reply@irq").with_data(1).and_succeed();

  ASSERT_FALSE(get_task().is_port_masked(p_tx));
}

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
