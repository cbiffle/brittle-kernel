#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <queue>
#include <stdexcept>

#include "protocol.h"


__attribute__((noreturn))
static void die(char const * msg) {
  perror(msg);
  exit(1);
}

struct Expectation {
  TaskMessageType req_type;
  Message req;

  SysMessageType resp_type;
  union {
    BasicResponse basic;
    MessageResponse msg;
  } resp;

  char const * expected_type() const {
    switch (req_type) {
      case TaskMessageType::send:
        return "send";
      case TaskMessageType::send_nb:
        return "non-blocking send";
      case TaskMessageType::call:
        return "call";
      case TaskMessageType::open_receive:
        return "open receive";
      default:
        return "???";
    }
  }

  void print_message(Message const & m) {
    fprintf(stderr, "- target   = %u\n", m.target);
    fprintf(stderr, "- mdata[0] = %u\n", m.mdata[0]);
    fprintf(stderr, "- mdata[1] = %u\n", m.mdata[1]);
    fprintf(stderr, "- mdata[2] = %u\n", m.mdata[2]);
    fprintf(stderr, "- mdata[3] = %u\n", m.mdata[3]);
  }

  void check_send(Message const & m) {
    bool ok = false;
    if (req_type == TaskMessageType::send) {
      if (req == m) ok = true;
    }

    if (!ok) {
      fprintf(stderr, "FAIL: got send, expected %s\n", expected_type());
      fprintf(stderr, "Expected message:\n");
      print_message(req);
      fprintf(stderr, "Actual message:\n");
      print_message(m);
      throw std::logic_error("test failed");
    }
  }

  void check_call(Message const & m) {
    bool ok = false;
    if (req_type == TaskMessageType::call) {
      if (req == m) ok = true;
    }

    if (!ok) {
      fprintf(stderr, "FAIL: got call, expected %s\n", expected_type());
      throw std::logic_error("test failed");
    }
  }

  void check_open_receive() {
    bool ok = false;
    if (req_type == TaskMessageType::open_receive) {
      ok = true;
    }

    if (!ok) {
      fprintf(stderr, "FAIL: got open receive, expected %s\n", expected_type());
      throw std::logic_error("test failed");
    }
  }

  void respond(int out) {
    write(out, &resp_type, sizeof(resp_type));
    switch (resp_type) {
      case SysMessageType::done_basic:
        write(out, &resp.basic, sizeof(resp.basic));
        break;

      case SysMessageType::done_message:
        write(out, &resp.msg, sizeof(resp.msg));
        break;
    }
  }
};

static std::queue<Expectation> xp;

struct Sender {
  Message req;

  void and_succeed() {
    and_return(0);
  }

  void and_return(unsigned status) {
    xp.push(Expectation {
        .req_type = TaskMessageType::send,
        .req = req,
        .resp_type = SysMessageType::done_basic,
        .resp = {
          .basic = {status},
        },
      });
  }
};

static Sender expect_send(unsigned target,
                          unsigned md0 = 0,
                          unsigned md1 = 0,
                          unsigned md2 = 0,
                          unsigned md3 = 0) {
  return Sender{Message{target, {md0, md1, md2, md3}}};
}

struct OpenReceiver {
  void and_fail(unsigned status) {
    xp.push(Expectation {
        .req_type = TaskMessageType::open_receive,
        .req = {},
        .resp_type = SysMessageType::done_message,
        .resp = {
          .msg = {
            .status = status,
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
        .req_type = TaskMessageType::open_receive,
        .req = {},
        .resp_type = SysMessageType::done_message,
        .resp = {
          .msg = {
            .status = 0,
            .brand = brand,
            .mdata = {md0, md1, md2, md3},
          },
        },
      });
  }
};

static OpenReceiver expect_open_receive() {
  return OpenReceiver{};
}

static bool rendezvous(int out, int in) {
  assert(!xp.empty());

  auto e = xp.front();
  xp.pop();

  TaskMessageType t;
  read(in, &t, sizeof(t));
  switch (t) {
    case TaskMessageType::send_nb:
    case TaskMessageType::send:
      fprintf(stderr, "SEND\n");
      {
        Message m;
        read(in, &m, sizeof(m));

        e.check_send(m);
        e.respond(out);
      }
      break;

    case TaskMessageType::call:
      fprintf(stderr, "CALL\n");
      {
        Message m;
        read(in, &m, sizeof(m));

        e.check_call(m);
        e.respond(out);
      }
      break;

    case TaskMessageType::open_receive:
      fprintf(stderr, "OPEN\n");
      {
        e.check_open_receive();
        e.respond(out);
      }
      break;
  }

  return true;
}

static void be_parent(int out, int in, pid_t child) {
  fprintf(stderr, "PARENT: starting sim loop\n");
  expect_open_receive().and_return(0, 0, 1, 2, 3);
  expect_send(0, 1, 2, 3, 0).and_succeed();

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
