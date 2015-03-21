#include <gtest/gtest.h>

#include "protocol.h"
#include "mock.h"

class UartTest : public ::sim::MockTest {
protected:
  UartTest() : MockTest("latest/sim/child") {}

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
};

TEST_F(UartTest, Heartbeat) {
  expect_open_receive()
      .and_provide(p_mon, {0, 1, 2, 3}, {"reply@hb"});

  expect_send_to("reply@hb", Blocking::no)
      .with_data(1, 2, 3)
      .and_succeed();
}

TEST_F(UartTest, SendByte) {
  expect_open_receive()
    .and_provide(1, {0, 0x42}, {"reply@send"});

  // Write data register.
  expect_send_to("hw")
    .with_data(0, 4, 0x42)
    .and_succeed();

  // Read, modify, write CR1
  expect_call_to("hw")
    .with_data(1, 0xC)
    .and_provide(0, {0xDEAD0000}, {});
  expect_send_to("hw")
    .with_data(0, 0xC, 0xDEAD0000 | (1 << 7))
    .and_succeed();

  // Final reply.
  expect_send_to("reply@send", Blocking::no).and_succeed();
  
  ASSERT_TRUE(get_task().is_port_masked(p_tx));

  /*
   * Interrupt when that byte hits the wire.
   */
  expect_open_receive()
    .and_provide(2, {}, {"reply@irq"});

  // Status register read.
  expect_call_to("hw")
    .with_data(1, 0)
    .and_provide(0, {0xBEEF0000 | (1 << 7)}, {});

  // Control register read.
  expect_call_to("hw")
    .with_data(1, 0xC)
    .and_provide(0, {0xF00D0000 | (1 << 7)}, {});

  // Control register write to squash interrupt.
  expect_send_to("hw")
    .with_data(0, 0xC, 0xF00D0000)
    .and_succeed();

  expect_send_to("reply@irq", Blocking::no)
    .with_data(1)
    .and_succeed();

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

  // Status register read.
  expect_call_to("hw")
    .with_data(1, 0)
    .and_provide(0, {0xBEEF0000 | (1 << 5)}, {});
  // Control register read.
  expect_call_to("hw")
    .with_data(1, 0xC)
    .and_provide(0, {0xF00D0000 | (1 << 5)}, {});
  // Control register write to squash interrupt.
  expect_send_to("hw")
    .with_data(0, 0xC, 0xF00D0000)
    .and_succeed();

  expect_send_to("reply@irq", Blocking::no)
    .with_data(1)
    .and_succeed();

  ASSERT_FALSE(get_task().is_port_masked(p_rx));

  /*
   * Receive a byte.
   */
  expect_open_receive()
    .and_provide(3, {0}, {"reply@rx"});

  // Read data register.
  expect_call_to("hw")
    .with_data(1, 4)
    .and_provide(0, {0x69}, {});

  // Read, modify, write CR1
  expect_call_to("hw")
    .with_data(1, 0xC)
    .and_provide(0, {0xDEAD0000}, {});
  expect_send_to("hw")
    .with_data(0, 0xC, 0xDEAD0000 | (1 << 5))
    .and_succeed();

  // Final reply.
  expect_send_to("reply@rx", Blocking::no)
    .with_data(0x69)
    .and_succeed();

  ASSERT_TRUE(get_task().is_port_masked(p_rx));
}

TEST_F(UartTest, FlushWhileIdle) {
  expect_open_receive()
    .and_provide(p_tx, {1}, {"reply@tx"});

  // Read, modify, write CR1 to enable TC interrupt.
  expect_call_to("hw")
    .with_data(1, 0xC)
    .and_provide(0, {0xDEAD0000}, {});
  expect_send_to("hw")
    .with_data(0, 0xC, 0xDEAD0000 | (1 << 6))
    .and_succeed();

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
  expect_call_to("hw")
    .with_data(1, 0)
    .and_provide(0, {0xBEEF0000 | (1 << 6)}, {});
  // Control register read.
  expect_call_to("hw")
    .with_data(1, 0xC)
    .and_provide(0, {0xF00D0000 | (1 << 6)}, {});
  // Control register write to squash interrupt.
  expect_send_to("hw")
    .with_data(0, 0xC, 0xF00D0000)
    .and_succeed();

  // Flush is finished, reply to saved cap.
  expect_send_to("reply@tx", Blocking::no)
    .and_succeed();

  // Reply to interrupt.
  expect_send_to("reply@irq", Blocking::no)
    .with_data(1)
    .and_succeed();

  ASSERT_FALSE(get_task().is_port_masked(p_tx));
}

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
