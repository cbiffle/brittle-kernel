#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <queue>
#include <stdexcept>

#include <gtest/gtest.h>

#include "protocol.h"
#include "mock.h"

class UartTest : public ::mock::MockTest {
protected:
  UartTest() : MockTest("latest/sim/child") {}

  static constexpr uintptr_t
      p_mon = 0,
      k0 = 0,
      k_saved_reply = 8;

  void SetUp() override {
    MockTest::SetUp();

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

#if 0
TEST_F(UartTest, SendByte) {
  expect_open_receive(true).and_return(1, 0, 0x42);

  // Move reply cap.
  expect_send(true, 15, 0, 0, 8).and_succeed();

  // Write data register.
  expect_send(true, 14, 0, 4, 0x42).and_succeed();

  // Mask transmit port
  expect_send(true, 15, 1, 4).and_succeed();

  // Read, modify, write CR1
  expect_call(true, 14, 1, 0xC).and_return(0, 0xDEAD0000);
  expect_send(true, 14, 0, 0xC, 0xDEAD0000 | (1 << 7)).and_succeed();

  // Final reply.
  expect_send(false, 8).and_succeed();

  /*
   * Interrupt when that byte hits the wire.
   */
  expect_open_receive(true).and_return(2);

  // Move reply cap.
  expect_send(true, 15, 0, 0, 8).and_succeed();
  // Status register read.
  expect_call(true, 14, 1, 0).and_return(0, 0xBEEF0000 | (1 << 7));
  // Control register read.
  expect_call(true, 14, 1, 0xC).and_return(0, 0xF00D0000 | (1 << 7));
  // Control register write to squash interrupt.
  expect_send(true, 14, 0, 0xC, 0xF00D0000).and_succeed();
  // Port unmask.
  expect_send(true, 15, 2, 4).and_succeed();

  expect_send(false, 8, 1).and_succeed();

  verify();
}

TEST_F(UartTest, ReceiveByte) {
  /*
   * Interrupt announcing receipt of a byte.
   */
  expect_open_receive(true).and_return(2);

  // Move reply cap.
  expect_send(true, 15, 0, 0, 8).and_succeed();
  // Status register read.
  expect_call(true, 14, 1, 0).and_return(0, 0xBEEF0000 | (1 << 5));
  // Control register read.
  expect_call(true, 14, 1, 0xC).and_return(0, 0xF00D0000 | (1 << 5));
  // Control register write to squash interrupt.
  expect_send(true, 14, 0, 0xC, 0xF00D0000).and_succeed();
  // Port unmask.
  expect_send(true, 15, 2, 5).and_succeed();
  expect_send(false, 8, 1).and_succeed();

  /*
   * Receive a byte.
   */
  expect_open_receive(true).and_return(3, 0);

  // Move reply cap.
  expect_send(true, 15, 0, 0, 8).and_succeed();

  // Read data register.
  expect_call(true, 14, 1, 4).and_return(0, 0x69);

  // Mask receive port
  expect_send(true, 15, 1, 5).and_succeed();

  // Read, modify, write CR1
  expect_call(true, 14, 1, 0xC).and_return(0, 0xDEAD0000);
  expect_send(true, 14, 0, 0xC, 0xDEAD0000 | (1 << 5)).and_succeed();

  // Final reply.
  expect_send(false, 8, 0x69).and_succeed();

  verify();
}

TEST_F(UartTest, FlushWhileIdle) {
  expect_open_receive(true).and_return(1, 1);

  // Move reply cap.
  expect_send(true, 15, 0, 0, 8).and_succeed();

  // Read, modify, write CR1 to enable TC interrupt.
  expect_call(true, 14, 1, 0xC).and_return(0, 0xDEAD0000);
  expect_send(true, 14, 0, 0xC, 0xDEAD0000 | (1 << 6)).and_succeed();

  // Mask transmit port
  expect_send(true, 15, 1, 4).and_succeed();

  // Move reply cap again.
  expect_send(true, 15, 0, 8, 9).and_succeed();

  // No reply just yet.

  // Interrupt!
  expect_open_receive(true).and_return(2);

  // Move reply cap.
  expect_send(true, 15, 0, 0, 8).and_succeed();
  // Status register read.
  expect_call(true, 14, 1, 0).and_return(0, 0xBEEF0000 | (1 << 6));
  // Control register read.
  expect_call(true, 14, 1, 0xC).and_return(0, 0xF00D0000 | (1 << 6));
  // Control register write to squash interrupt.
  expect_send(true, 14, 0, 0xC, 0xF00D0000).and_succeed();

  // Flush is finished, reply to saved cap.
  expect_send(false, 9).and_succeed();

  // Port unmask.
  expect_send(true, 15, 2, 4).and_succeed();

  // Reply to interrupt.
  expect_send(false, 8, 1).and_succeed();

  verify();
}

#endif

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
