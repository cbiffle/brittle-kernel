#include "mock.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>

#include <queue>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include "protocol.h"

namespace mock {

static constexpr bool trace = false;


/*******************************************************************************
 * Task model.
 */

__attribute__((noreturn))
static void die(char const * msg) {
  perror(msg);
  exit(1);
}

Task::Task(char const * child_path)
  : _child_path(child_path),
    _in(-1),
    _out(-1),
    _child(-1) {}

void Task::start() {
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

void Task::stop() {
  close(_out);
  close(_in);
  kill(_child, 9);
}

void Task::wait_for_syscall() {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(_in, &readfds);

  while (true) {
    int r = select(_in + 1, &readfds, nullptr, nullptr, nullptr);
    switch (r) {
      case 0: continue;
      case 1: return;
      default:
        fprintf(stderr, "WARN: failure in select\n");
        return;
    }
  }
}

std::string Task::get_key(uintptr_t index) {
  auto it = _clist.find(index);
  if (it == _clist.end()) {
    return "";
  } else {
    return it->second;
  }
}

void Task::set_key(uintptr_t index, std::string name) {
  _clist[index] = name;
}

MessageKeyNames Task::get_pass_keys() {
  return {
    get_key(0),
    get_key(1),
    get_key(2),
    get_key(3),
  };
}

void Task::set_pass_keys(MessageKeyNames const & k) {
  for (unsigned i = 0; i < n_keys_sent; ++i) {
    _clist[i] = k.name[i];
  }
}

void Task::clear_pass_keys() {
  set_pass_keys({"","","",""});
}

void Task::sim_sys(SendRequest const & r) {
  switch (r.m.data[0]) {
    case 0:  // move cap
      set_key(r.m.data[2], get_key(r.m.data[1]));
      set_key(r.m.data[1], "");

      out(ResponseType::complete);
      out(CompleteResponse { SysResult::success });
      break;

    case 1:  // mask port
      _masked_ports.insert(r.m.data[1]);
      out(ResponseType::complete);
      out(CompleteResponse { SysResult::success });
      break;

    case 2:  // unmask port
      _masked_ports.erase(r.m.data[1]);
      out(ResponseType::complete);
      out(CompleteResponse { SysResult::success });
      break;

    default:
      FAIL() << "Bad message to kernel: " << r.m.data[0];
      throw std::logic_error("test failed");
  }
}

void Task::revoke_key(std::string const & k) {
  for (unsigned i = 0; i < n_keys_sent; ++i) {
    if (get_key(i) == k) {
      set_key(i, "<REVOKED>");
    }
  }

}

void Task::sim_sys(CallRequest const & r) {
  switch (r.m.data[0]) {
    case 0:  // move cap
      set_key(r.m.data[2], get_key(r.m.data[1]));
      set_key(r.m.data[1], "");

      out(ResponseType::message);
      out(MessageResponse {
          .rm = {
            .brand = 0,
            .m = {0,0,0,0},
          },
          });
      clear_pass_keys();
      break;

    case 1:  // mask port
      _masked_ports.insert(r.m.data[1]);
      out(ResponseType::message);
      out(MessageResponse {
          .rm = {
            .brand = 0,
            .m = {0,0,0,0},
          },
          });
      clear_pass_keys();
      break;

    case 2:  // unmask port
      _masked_ports.erase(r.m.data[1]);
      out(ResponseType::message);
      out(MessageResponse {
          .rm = {
            .brand = 0,
            .m = {0,0,0,0},
          },
          });
      clear_pass_keys();
      break;

    default:
      FAIL() << "Bad message to kernel: " << r.m.data[0];
  }
}

bool Task::is_port_masked(uintptr_t p) {
  return _masked_ports.find(p) != _masked_ports.end();
}


/*******************************************************************************
 * Test support.
 */

static void print_message(Message const & m) {
  fprintf(stderr, "- data[0] = %lu\n", m.data[0]);
  fprintf(stderr, "- data[1] = %lu\n", m.data[1]);
  fprintf(stderr, "- data[2] = %lu\n", m.data[2]);
  fprintf(stderr, "- data[3] = %lu\n", m.data[3]);
}

static void print_keys(MessageKeyNames const & k) {
  fprintf(stderr, "- key[0] = '%s'\n", k.name[0].c_str());
  fprintf(stderr, "- key[1] = '%s'\n", k.name[1].c_str());
  fprintf(stderr, "- key[2] = '%s'\n", k.name[2].c_str());
  fprintf(stderr, "- key[3] = '%s'\n", k.name[3].c_str());
}

/*
static void print_call_request(CallRequest const & r) {
  fprintf(stderr, "- CALL\n");
  fprintf(stderr, "- block   = %u\n", r.blocking);
  fprintf(stderr, "- target  = %lu\n", r.target);
  print_message(r.m);
}
*/

static void print_req(SendRequest const & r, Task & task) {
  fprintf(stderr, "- SEND\n");
  fprintf(stderr, "- block  = %s\n", r.blocking ? "true" : "false");
  fprintf(stderr, "- target = '%s'\n", task.get_key(r.target).c_str());
  print_message(r.m);
  print_keys(task.get_pass_keys());
}

static void print_req(CallRequest const & r, Task & task) {
  fprintf(stderr, "- CALL\n");
  fprintf(stderr, "- block  = %s\n", r.blocking ? "true" : "false");
  fprintf(stderr, "- target = '%s'\n", task.get_key(r.target).c_str());
  print_message(r.m);
  print_keys(task.get_pass_keys());
}

static void print_req(OpenReceiveRequest const & r, Task & task) {
  fprintf(stderr, "- OPEN RECEIVE\n");
  fprintf(stderr, "- block  = %s\n", r.blocking ? "true" : "false");
}

static void print_incoming(RequestType rt, Task & task) {
  switch (rt) {
    case RequestType::send:
      {
        auto r = task.in<SendRequest>();
        print_req(r, task);
        break;
      }
    case RequestType::call:
      {
        auto r = task.in<CallRequest>();
        print_req(r, task);
        break;
      }
    case RequestType::open_receive:
      {
        auto r = task.in<OpenReceiveRequest>();
        print_req(r, task);
        break;
      }
  }
}

SendBuilder::SendBuilder(bool blocking, std::string const & target, Task & t)
  : _blocking(blocking),
    _target(target),
    _task(t) {}

SendBuilder & SendBuilder::with_data(uintptr_t md0,
                                     uintptr_t md1,
                                     uintptr_t md2,
                                     uintptr_t md3) {
  _m = Message{md0,md1,md2,md3};
  _message_matters = true;
  return *this;
}

SendBuilder & SendBuilder::with_keys(const char * k0,
                                     const char * k1,
                                     const char * k2,
                                     const char * k3) {
  _k = MessageKeyNames{k0, k1, k2, k3};
  _keys_matter = true;
  return *this;
}

void SendBuilder::and_succeed() {
  and_return(SysResult::success);
}

void SendBuilder::and_return(SysResult result) {
  while (true) {
    auto rt = _task.in<RequestType>();
    if (rt == RequestType::send) {
      auto r = _task.in<SendRequest>();

      if (_task.get_key(r.target) == "<SYS>") {
        _task.sim_sys(r);
        continue;
      }

      bool ok = true;
      if (_blocking != r.blocking) ok = false;
      if (_target != _task.get_key(r.target)) ok = false;
      if (_message_matters && _m != r.m) ok = false;
      if (_keys_matter && _k != _task.get_pass_keys()) ok = false;
      if (!ok) {
        fprintf(stderr, "FAIL: send parameters bad\n");
        fprintf(stderr, "Expected:\n");
        print();
        fprintf(stderr, "Actual:\n");
        print_req(r, _task);
        throw std::logic_error("test failed");
      } else {
        _task.out(ResponseType::complete);
        _task.out(CompleteResponse { SysResult::success });

        std::string reply_prefix("reply@");
        if (_target.compare(0, reply_prefix.length(), reply_prefix) == 0) {
          _task.revoke_key(_target);
        }
        return;
      }
    } else {
      if (rt == RequestType::call) {
        auto r = _task.in<CallRequest>();
        if (_task.get_key(r.target) == "<SYS>") {
          _task.sim_sys(r);
          continue;
        }
        fprintf(stderr, "FAIL: expected send, got something else.\n");
        fprintf(stderr, "Expected:\n");
        print();
        fprintf(stderr, "Actual:\n");
        print_req(r, _task);
        throw std::logic_error("test failed");
      }
      fprintf(stderr, "FAIL: expected send, got something else.\n");
      fprintf(stderr, "Expected:\n");
      print();
      fprintf(stderr, "Actual type: %u\n", unsigned(rt));
      throw std::logic_error("test failed");
    }
  }
}

void SendBuilder::print() {
  fprintf(stderr, "SEND:\n");
  fprintf(stderr, "- block  = %u\n", _blocking);
  fprintf(stderr, "- target = %s\n", _target.c_str());
  print_message(_m);
  print_keys(_k);
}

CallBuilder::CallBuilder(bool blocking, std::string const & target, Task & t)
  : _blocking(blocking),
    _target(target),
    _task(t) {}

CallBuilder & CallBuilder::with_data(uintptr_t md0,
                                     uintptr_t md1,
                                     uintptr_t md2,
                                     uintptr_t md3) {
  _m_out = Message{md0,md1,md2,md3};
  _message_matters = true;
  return *this;
}

CallBuilder & CallBuilder::with_keys(const char * k0,
                                     const char * k1,
                                     const char * k2,
                                     const char * k3) {
  _k_out = MessageKeyNames{k0, k1, k2, k3};
  _keys_matter = true;
  return *this;
}

void CallBuilder::and_provide(unsigned brand,
                              Message m_in,
                              MessageKeyNames k_in) {
  while (true) {
    auto rt = _task.in<RequestType>();
    if (rt == RequestType::call) {
      auto r = _task.in<CallRequest>();

      if (_task.get_key(r.target) == "<SYS>") {
        _task.sim_sys(r);
        continue;
      }

      bool ok = true;
      if (_blocking != r.blocking) ok = false;
      if (_target != _task.get_key(r.target)) ok = false;
      if (_message_matters && _m_out != r.m) ok = false;
      if (_keys_matter && _k_out != _task.get_pass_keys()) ok = false;
      if (!ok) {
        fprintf(stderr, "FAIL: call parameters bad\n");
        fprintf(stderr, "Expected:\n");
        print();
        fprintf(stderr, "Actual:\n");
        print_req(r, _task);
        throw std::logic_error("test failed");
      } else {
        _task.out(ResponseType::message);
        _task.out(MessageResponse {
            .rm = {
              .brand = brand,
              .m = m_in,
            },
          });
        _task.set_pass_keys(k_in);

        std::string reply_prefix("reply@");
        if (_target.compare(0, reply_prefix.length(), reply_prefix) == 0) {
          _task.revoke_key(_target);
        }
        return;
      }
    } else {
      if (rt == RequestType::send) {
        auto r = _task.in<SendRequest>();
        if (_task.get_key(r.target) == "<SYS>") {
          _task.sim_sys(r);
          continue;
        }
        fprintf(stderr, "FAIL: expected call, got something else.\n");
        fprintf(stderr, "Expected:\n");
        print();
        fprintf(stderr, "Actual:\n");
        print_req(r, _task);
        throw std::logic_error("test failed");
      } else {
        fprintf(stderr, "FAIL: expected call, got something else.\n");
        fprintf(stderr, "Expected:\n");
        print();
        fprintf(stderr, "Actual type: %u\n", unsigned(rt));
        print_incoming(rt, _task);
        throw std::logic_error("test failed");
      }
    }
  }
}

void CallBuilder::print() {
  fprintf(stderr, "CALL:\n");
  fprintf(stderr, "- block  = %u\n", _blocking);
  fprintf(stderr, "- target = %s\n", _target.c_str());
  print_message(_m_out);
  print_keys(_k_out);
}


/*
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

*/

OpenReceiveBuilder::OpenReceiveBuilder(bool blocking, Task & task)
  : _blocking(blocking),
    _task(task) {}

void OpenReceiveBuilder::and_provide(unsigned brand,
                                     Message m,
                                     MessageKeyNames k) {
  while (true) {
    auto rt = _task.in<RequestType>();
    if (rt == RequestType::open_receive) {
      auto r = _task.in<OpenReceiveRequest>();
      bool ok = true;
      if (_blocking != r.blocking) ok = false;
      if (!ok) {
        fprintf(stderr, "FAIL: open receive parameters bad\n");
        fprintf(stderr, "Expected:\n");
        print();
        fprintf(stderr, "Actual:\n");
        print_req(r, _task);
        throw std::logic_error("test failed");
      } else {
        _task.out(ResponseType::message);
        _task.out(MessageResponse {
            .rm = {
            .brand = brand,
            .m = m,
            },
            });
        _task.set_pass_keys(k);
      }
      return;
    } else {
      if (rt == RequestType::send) {
        auto r = _task.in<SendRequest>();

        if (_task.get_key(r.target) == "<SYS>") {
          _task.sim_sys(r);
          continue;
        }
      }
      fprintf(stderr, "FAIL: expected open receive, got something else.\n");
      fprintf(stderr, "Expected:\n");
      print();
      fprintf(stderr, "Actual type: %u\n", unsigned(rt));
      throw std::logic_error("test failed");
    }
  }
}

void OpenReceiveBuilder::print() {
  fprintf(stderr, "OPEN RECEIVE:\n");
  fprintf(stderr, "- block  = %u\n", _blocking);
}


/*******************************************************************************
 * Actual test
 */

MockTest::MockTest(char const * child_path)
  : _task(child_path) {}

void MockTest::SetUp() {
  _task.start();
}

void MockTest::TearDown() {
  _task.stop();
}

SendBuilder MockTest::expect_send_to(std::string target,
                                     Blocking blocking) {
  return SendBuilder{blocking == Blocking::yes, target, _task};
}

CallBuilder MockTest::expect_call_to(std::string target,
                                     Blocking blocking) {
  return CallBuilder{blocking == Blocking::yes, target, _task};
}

OpenReceiveBuilder MockTest::expect_open_receive(Blocking blocking) {
  return OpenReceiveBuilder{blocking == Blocking::yes, _task};
}

}  // namespace mock
