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

static constexpr bool trace = false;

/*******************************************************************************
 * Protocol runtime
 */

template <typename T>
static T in(int fd) {
  T tmp;
  if (read(fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
    perror("reading from child");
    throw std::logic_error("I/O");
  }
  return tmp;
}

template <typename T>
static void out(int fd, T const & data) {
  if (write(fd, &data, sizeof(data)) != sizeof(data)) {
    perror("writing to child");
    throw std::logic_error("I/O");
  }
}



/*******************************************************************************
 * Test support.
 */

__attribute__((noreturn))
static void die(char const * msg) {
  perror(msg);
  exit(1);
}

struct Expectation {
  RequestType req_type;
  union {
    SendRequest send;
    CallRequest call;
    OpenReceiveRequest open_receive;
  } req;

  ResponseType resp_type;
  union {
    MessageResponse message;
    CompleteResponse complete;
  } resp;

  char const * expected_type() const {
    switch (req_type) {
      case RequestType::send:
        return "send";
      case RequestType::call:
        return "call";
      case RequestType::open_receive:
        return "open receive";
      default:
        return "???";
    }
  }

  void print_send_request(SendRequest const & s) {
    fprintf(stderr, "- SEND\n");
    fprintf(stderr, "- block   = %u\n", s.blocking);
    fprintf(stderr, "- target  = %lu\n", s.target);
    print_message(s.m);
  }

  void print_call_request(CallRequest const & r) {
    fprintf(stderr, "- CALL\n");
    fprintf(stderr, "- block   = %u\n", r.blocking);
    fprintf(stderr, "- target  = %lu\n", r.target);
    print_message(r.m);
  }

  void print_open_receive_request(OpenReceiveRequest const & r) {
    fprintf(stderr, "- OPEN_RECEIVE\n");
    fprintf(stderr, "- block   = %u\n", r.blocking);
  }

  void print_message(Message const & m) {
    fprintf(stderr, "- data[0] = %lu\n", m.data[0]);
    fprintf(stderr, "- data[1] = %lu\n", m.data[1]);
    fprintf(stderr, "- data[2] = %lu\n", m.data[2]);
    fprintf(stderr, "- data[3] = %lu\n", m.data[3]);
  }

  void print_request() {
    fprintf(stderr, "Expected request:\n");
    switch (req_type) {
      case RequestType::send:
        print_send_request(req.send);
        break;
      case RequestType::call:
        print_call_request(req.call);
        break;
      case RequestType::open_receive:
        print_open_receive_request(req.open_receive);
        break;
    }
  }

  void check_send(SendRequest const & actual) {
    bool ok = true;
    if (req_type == RequestType::send) {
      if (req.send.blocking != actual.blocking) ok = false;
      if (req.send.target != actual.target) ok = false;
      if (req.send.m != actual.m) ok = false;
      if (!ok) {
        fprintf(stderr, "FAIL: send parameters bad\n");
      }
    } else {
      fprintf(stderr, "FAIL: got send\n");
      ok = false;
    }

    if (!ok) {
      print_request();
      fprintf(stderr, "Actual request:\n");
      print_send_request(actual);
      throw std::logic_error("test failed");
    }
  }

  void check_call(CallRequest const &actual) {
    bool ok = true;
    if (req_type == RequestType::call) {
      if (req.send.blocking != actual.blocking) ok = false;
      if (req.send.target != actual.target) ok = false;
      if (req.send.m != actual.m) ok = false;
      if (!ok) {
        fprintf(stderr, "FAIL: call parameters bad\n");
      }
    } else {
      fprintf(stderr, "FAIL: got call\n");
      ok = false;
    }

    if (!ok) {
      print_request();
      fprintf(stderr, "Actual request:\n");
      print_call_request(actual);
      throw std::logic_error("test failed");
    }
  }

  void check_open_receive(OpenReceiveRequest const &actual) {
    bool ok = true;
    if (req_type == RequestType::open_receive) {
      if (req.send.blocking != actual.blocking) ok = false;
      if (!ok) {
        fprintf(stderr, "FAIL: open receive parameters bad\n");
      }
    } else {
      fprintf(stderr, "FAIL: got open receive\n");
      ok = false;
    }

    if (!ok) {
      print_request();
      fprintf(stderr, "Actual request:\n");
      print_open_receive_request(actual);
      throw std::logic_error("test failed");
    }
  }

  void respond(int outf) {
    out(outf, resp_type);
    switch (resp_type) {
      case ResponseType::message:
        out(outf, resp.message);
        break;

      case ResponseType::complete:
        out(outf, resp.complete);
        break;
    }
  }
};

struct Sender {
  SendRequest req;
  std::queue<Expectation> & xp;

  void and_succeed() {
    and_return(SysResult::success);
  }

  void and_return(SysResult result) {
    xp.push(Expectation {
        .req_type = RequestType::send,
        .req = {
          .send = req,
        },
        .resp_type = ResponseType::complete,
        .resp = {
          .complete = {result},
        },
      });
  }
};

struct Caller {
  CallRequest req;
  std::queue<Expectation> & xp;

  void and_return(uintptr_t brand,
                  uintptr_t md0 = 0,
                  uintptr_t md1 = 0,
                  uintptr_t md2 = 0,
                  uintptr_t md3 = 0) {
    xp.push(Expectation {
        .req_type = RequestType::call,
        .req = {
          .call = req,
        },
        .resp_type = ResponseType::message,
        .resp = {
          .message = {
            .rm = {
              .brand = brand,
              .m = {{md0, md1, md2, md3}},
            },
          },
        },
      });
  }
};

struct OpenReceiver {
  OpenReceiveRequest req;
  std::queue<Expectation> & xp;

  void and_fail(SysResult result) {
    xp.push(Expectation {
        .req_type = RequestType::open_receive,
        .req = {
          .open_receive = req,
        },
        .resp_type = ResponseType::complete,
        .resp = {
          .complete = {
            .result = result,
          },
        },
      });
  }

  void and_return(unsigned brand,
                  unsigned md0 = 0,
                  unsigned md1 = 0,
                  unsigned md2 = 0,
                  unsigned md3 = 0) {
    xp.push(Expectation {
        .req_type = RequestType::open_receive,
        .req = {
          .open_receive = req,
        },
        .resp_type = ResponseType::message,
        .resp = {
          .message = {
            .rm = {
              .brand = brand,
              .m = {{md0, md1, md2, md3}},
            },
          },
        },
      });
  }
};


/*******************************************************************************
 * Actual test
 */

class SimTest : public ::testing::Test {
protected:
  void SetUp() override {
    _verified = false;

    int p2c[2];
    int c2p[2];
    if (pipe(p2c) < 0) die("pipe");
    if (pipe(c2p) < 0) die("pipe");

    switch ((_child = fork())) {
      case -1:
        die("fork");

      case 0:  // child
        {
          close(0);
          close(1);

          if (dup2(p2c[0], 0) < 0) die("dup2");
          if (dup2(c2p[1], 1) < 0) die("dup2");
          close(p2c[0]);
          close(p2c[1]);
          close(c2p[0]);
          close(c2p[1]);

          char const * const args[] = {
            "latest/sim/child",
            nullptr,
          };
          if (execve("latest/sim/child",
                const_cast<char * const *>(args),
                environ) < 0) die("execve");
          // Does not return otherwise.
          while (true);
        }

      default:  // parent
        _out = p2c[1];
        _in = c2p[0];
        break;
    }
  }

  void TearDown() override {
    ASSERT_TRUE(_verified) << "forgot to call verify!";
    close(_out);
    close(_in);
    kill(_child, 9);
  }

  Sender expect_send(bool block,
                     unsigned target,
                     unsigned md0 = 0,
                     unsigned md1 = 0,
                     unsigned md2 = 0,
                     unsigned md3 = 0) {
    return Sender{
      SendRequest{block, target, Message{{md0, md1, md2, md3}}},
      _xp,
    };
  }

  Caller expect_call(bool block,
                     unsigned target,
                     unsigned md0 = 0,
                     unsigned md1 = 0,
                     unsigned md2 = 0,
                     unsigned md3 = 0) {
    return Caller{
      CallRequest{block, target, Message{{md0, md1, md2, md3}}},
      _xp,
    };
  }

  OpenReceiver expect_open_receive(bool block) {
    return OpenReceiver{
      .req = { .blocking = block },
      .xp = _xp,
    };
  }

  void verify() {
    while (!_xp.empty()) rendezvous();
    _verified = true;
  }

  bool rendezvous() {
    assert(!_xp.empty());

    auto e = _xp.front();
    _xp.pop();

    auto t = in<RequestType>(_in);
    switch (t) {
      case RequestType::send:
        if (trace) fprintf(stderr, "SEND\n");
        {
          e.check_send(in<SendRequest>(_in));
          e.respond(_out);
        }
        break;

      case RequestType::call:
        if (trace) fprintf(stderr, "CALL\n");
        {
          e.check_call(in<CallRequest>(_in));
          e.respond(_out);
        }
        break;

      case RequestType::open_receive:
        if (trace) fprintf(stderr, "OPEN\n");
        {
          e.check_open_receive(in<OpenReceiveRequest>(_in));
          e.respond(_out);
        }
        break;
    }

    return true;
  }


private:
  int _out;
  int _in;
  pid_t _child;
  std::queue<Expectation> _xp;
  bool _verified;
};

TEST_F(SimTest, Heartbeat) {
  expect_open_receive(true).and_return(0, 0, 1, 2, 3);
  expect_send(true, 15, 0, 0, 8).and_succeed();
  expect_send(false, 8, 1, 2, 3, 0).and_succeed();

  verify();
}

TEST_F(SimTest, SendByte) {
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

TEST_F(SimTest, ReceiveByte) {
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

TEST_F(SimTest, FlushWhileIdle) {
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



int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
