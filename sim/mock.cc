#include "mock.h"

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

namespace mock {

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

void Expectation::print_send_request(SendRequest const & s) {
  fprintf(stderr, "- SEND\n");
  fprintf(stderr, "- block   = %u\n", s.blocking);
  fprintf(stderr, "- target  = %lu\n", s.target);
  print_message(s.m);
}

void Expectation::print_call_request(CallRequest const & r) {
  fprintf(stderr, "- CALL\n");
  fprintf(stderr, "- block   = %u\n", r.blocking);
  fprintf(stderr, "- target  = %lu\n", r.target);
  print_message(r.m);
}

void Expectation::print_open_receive_request(OpenReceiveRequest const & r) {
  fprintf(stderr, "- OPEN_RECEIVE\n");
  fprintf(stderr, "- block   = %u\n", r.blocking);
}

void Expectation::print_message(Message const & m) {
  fprintf(stderr, "- data[0] = %lu\n", m.data[0]);
  fprintf(stderr, "- data[1] = %lu\n", m.data[1]);
  fprintf(stderr, "- data[2] = %lu\n", m.data[2]);
  fprintf(stderr, "- data[3] = %lu\n", m.data[3]);
}

void Expectation::print_request() {
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

void Expectation::check_send(SendRequest const & actual) {
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

void Expectation::check_call(CallRequest const &actual) {
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

void Expectation::check_open_receive(OpenReceiveRequest const &actual) {
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

void Expectation::respond(int outf) {
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

void Sender::and_succeed() {
  and_return(SysResult::success);
}

void Sender::and_return(SysResult result) {
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

void Caller::and_return(uintptr_t brand,
                        uintptr_t md0,
                        uintptr_t md1,
                        uintptr_t md2,
                        uintptr_t md3) {
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

void OpenReceiver::and_fail(SysResult result) {
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

void OpenReceiver::and_return(unsigned brand,
                              unsigned md0,
                              unsigned md1,
                              unsigned md2,
                              unsigned md3) {
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


/*******************************************************************************
 * Actual test
 */

MockTest::MockTest(char const * child_path)
  : _out(-1),
    _in(-1),
    _child(-1),
    _xp(),
    _verified(false),
    _child_path(child_path) {}

void MockTest::SetUp() {
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
          _child_path,
          nullptr,
        };
        if (execve(_child_path,
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

void MockTest::TearDown() {
  ASSERT_TRUE(_verified) << "forgot to call verify!";
  close(_out);
  close(_in);
  kill(_child, 9);
}

Sender MockTest::expect_send(bool block,
                             unsigned target,
                             unsigned md0,
                             unsigned md1,
                             unsigned md2,
                             unsigned md3) {
  return Sender{
    SendRequest{block, target, Message{{md0, md1, md2, md3}}},
    _xp,
  };
}

Caller MockTest::expect_call(bool block,
                             unsigned target,
                             unsigned md0,
                             unsigned md1,
                             unsigned md2,
                             unsigned md3) {
  return Caller{
    CallRequest{block, target, Message{{md0, md1, md2, md3}}},
    _xp,
  };
}

OpenReceiver MockTest::expect_open_receive(bool block) {
  return OpenReceiver{
    .req = { .blocking = block },
    .xp = _xp,
  };
}

void MockTest::verify() {
  while (!_xp.empty()) rendezvous();
  _verified = true;
}

bool MockTest::rendezvous() {
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

}  // namespace mock
