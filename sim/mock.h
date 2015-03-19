#ifndef SIM_MOCK_H
#define SIM_MOCK_H

#include <queue>

#include <gtest/gtest.h>

#include "protocol.h"

namespace mock {

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

  void print_send_request(SendRequest const & s);

  void print_call_request(CallRequest const & r);

  void print_open_receive_request(OpenReceiveRequest const & r);

  void print_message(Message const & m);

  void print_request();

  void check_send(SendRequest const & actual);

  void check_call(CallRequest const &actual);

  void check_open_receive(OpenReceiveRequest const &actual);

  void respond(int outf);
};

struct Sender {
  SendRequest req;
  std::queue<Expectation> & xp;

  void and_succeed();
  void and_return(SysResult result);
};

struct Caller {
  CallRequest req;
  std::queue<Expectation> & xp;

  void and_return(uintptr_t brand,
                  uintptr_t md0 = 0,
                  uintptr_t md1 = 0,
                  uintptr_t md2 = 0,
                  uintptr_t md3 = 0);
};

struct OpenReceiver {
  OpenReceiveRequest req;
  std::queue<Expectation> & xp;

  void and_fail(SysResult result);

  void and_return(unsigned brand,
                  unsigned md0 = 0,
                  unsigned md1 = 0,
                  unsigned md2 = 0,
                  unsigned md3 = 0);
};


/*******************************************************************************
 * Actual test
 */

class MockTest : public ::testing::Test {
protected:
  MockTest(char const * child_path);

  void SetUp() override;
  void TearDown() override;

  Sender expect_send(bool block,
                     unsigned target,
                     unsigned md0 = 0,
                     unsigned md1 = 0,
                     unsigned md2 = 0,
                     unsigned md3 = 0);

  Caller expect_call(bool block,
                     unsigned target,
                     unsigned md0 = 0,
                     unsigned md1 = 0,
                     unsigned md2 = 0,
                     unsigned md3 = 0);

  OpenReceiver expect_open_receive(bool block);

  void verify();

  bool rendezvous();

private:
  int _out;
  int _in;
  pid_t _child;
  std::queue<Expectation> _xp;
  bool _verified;
  char const * _child_path;
};

}  // namespace mock

#endif  // SIM_MOCK_H
