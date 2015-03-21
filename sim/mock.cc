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

namespace sim {

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

static void print_req(CopyCapRequest const & r, Task & task) {
  fprintf(stderr, "- COPY CAP\n");
  fprintf(stderr, "- from   = %lu ('%s')\n",
      r.from_index,
      task.get_key(r.from_index).c_str());
  fprintf(stderr, "- to     = %lu\n", r.to_index);
}

static void print_req(NullCapRequest const & r, Task & task) {
  fprintf(stderr, "- NULL CAP\n");
  fprintf(stderr, "- index  = %lu\n", r.index);
}

static void print_req(MaskPortRequest const & r, Task & task) {
  fprintf(stderr, "- MASK PORT\n");
  fprintf(stderr, "- index  = %lu\n", r.index);
}

static void print_req(UnmaskPortRequest const & r, Task & task) {
  fprintf(stderr, "- UNMASK PORT\n");
  fprintf(stderr, "- index  = %lu\n", r.index);
}

static void print_incoming(RequestType rt, Task & task) {
  switch (rt) {

#define READ_AND_PRINT_REQ(name_lo, name_up) \
    case RequestType::name_lo: \
      { \
        auto r = task.in<name_up>(); \
        print_req(r, task); \
        break; \
      }

    READ_AND_PRINT_REQ(send, SendRequest);
    READ_AND_PRINT_REQ(call, CallRequest);
    READ_AND_PRINT_REQ(open_receive, OpenReceiveRequest);
    READ_AND_PRINT_REQ(copy_cap, CopyCapRequest);
    READ_AND_PRINT_REQ(null_cap, NullCapRequest);
    READ_AND_PRINT_REQ(mask_port, MaskPortRequest);
    READ_AND_PRINT_REQ(unmask_port, UnmaskPortRequest);

    default:
      fprintf(stderr, "(corrupt type)\n");
      break;
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
    auto rt = _task.next_nontrivial_syscall();
    if (rt == RequestType::send) {
      auto r = _task.in<SendRequest>();

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
    auto rt = _task.next_nontrivial_syscall();
    if (rt == RequestType::call) {
      auto r = _task.in<CallRequest>();

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
      fprintf(stderr, "FAIL: expected call, got something else.\n");
      fprintf(stderr, "Expected:\n");
      print();
      fprintf(stderr, "Actual type: %u\n", unsigned(rt));
      print_incoming(rt, _task);
      throw std::logic_error("test failed");
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

OpenReceiveBuilder::OpenReceiveBuilder(bool blocking, Task & task)
  : _blocking(blocking),
    _task(task) {}

void OpenReceiveBuilder::and_provide(unsigned brand,
                                     Message m,
                                     MessageKeyNames k) {
  while (true) {
    auto rt = _task.next_nontrivial_syscall();
    if (rt == RequestType::open_receive) {
      if (_task.is_port_masked(brand)) {
        fprintf(stderr, "FAIL: task has masked port %u, cannot deliver.\n",
            brand);
        throw std::logic_error("test failed");
      }

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

}  // namespace sim
