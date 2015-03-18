#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <queue>
#include <stdexcept>

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

static std::queue<Expectation> xp;

struct Sender {
  SendRequest req;

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

static Sender expect_send(bool block,
                          unsigned target,
                          unsigned md0 = 0,
                          unsigned md1 = 0,
                          unsigned md2 = 0,
                          unsigned md3 = 0) {
  return Sender{SendRequest{block, target, Message{{md0, md1, md2, md3}}}};
}

struct Caller {
  CallRequest req;

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

static Caller expect_call(bool block,
                          unsigned target,
                          unsigned md0 = 0,
                          unsigned md1 = 0,
                          unsigned md2 = 0,
                          unsigned md3 = 0) {
  return Caller{CallRequest{block, target, Message{{md0, md1, md2, md3}}}};
}

struct OpenReceiver {
  OpenReceiveRequest req;

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

static OpenReceiver expect_open_receive(bool block) {
  return OpenReceiver{
    .req = { .blocking = block },
  };
}

static bool rendezvous(int outf, int inf) {
  assert(!xp.empty());

  auto e = xp.front();
  xp.pop();

  auto t = in<RequestType>(inf);
  switch (t) {
    case RequestType::send:
      if (trace) fprintf(stderr, "SEND\n");
      {
        e.check_send(in<SendRequest>(inf));
        e.respond(outf);
      }
      break;

    case RequestType::call:
      if (trace) fprintf(stderr, "CALL\n");
      {
        e.check_call(in<CallRequest>(inf));
        e.respond(outf);
      }
      break;

    case RequestType::open_receive:
      if (trace) fprintf(stderr, "OPEN\n");
      {
        e.check_open_receive(in<OpenReceiveRequest>(inf));
        e.respond(outf);
      }
      break;
  }

  return true;
}


/*******************************************************************************
 * Actual test
 */

static void be_parent(int out, int in, pid_t child) {
  if (trace) fprintf(stderr, "PARENT: starting sim loop\n");

  /*
   * Monitoring message
   */
  expect_open_receive(true).and_return(0, 0, 1, 2, 3);
  expect_send(true, 15, 0, 0, 8).and_succeed(); 
  expect_send(false, 8, 1, 2, 3, 0).and_succeed();

  /*
   * Send a byte.
   */
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


  while (!xp.empty()) {
    rendezvous(out, in);
  }
  fprintf(stderr, "PARENT: test completed.\n");
}

static pid_t child;

static void kill_child() {
  kill(child, 9);
}

int main() {
  
  int p2c[2];
  int c2p[2];
  if (pipe(p2c) < 0) die("pipe");
  if (pipe(c2p) < 0) die("pipe");

  switch ((child = fork())) {
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
      atexit(kill_child);
      be_parent(p2c[1], c2p[0], child);
      break;
  }

  return 0;
}
