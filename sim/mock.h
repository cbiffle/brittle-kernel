#ifndef SIM_MOCK_H
#define SIM_MOCK_H

#include <map>
#include <memory>
#include <set>

#include <gtest/gtest.h>

#include "config.h"
#include "protocol.h"
#include "task.h"

namespace sim {

class SendBuilder {
public:
  SendBuilder(bool blocking, std::string const & target, Task &);
  SendBuilder & with_data(uintptr_t = 0,
                          uintptr_t = 0,
                          uintptr_t = 0,
                          uintptr_t = 0);
  SendBuilder & with_keys(char const * = "",
                          char const * = "",
                          char const * = "",
                          char const * = "");
  void and_succeed();
  void and_return(SysResult result);

private:
  bool _blocking;
  std::string _target;
  bool _message_matters = false;
  Message _m;
  bool _keys_matter = false;
  MessageKeyNames _k;

  Task & _task;

  void print();
};

class CallBuilder {
public:
  CallBuilder(bool blocking, std::string const & target, Task &);
  CallBuilder & with_data(uintptr_t = 0,
                          uintptr_t = 0,
                          uintptr_t = 0,
                          uintptr_t = 0);
  CallBuilder & with_keys(char const * = "",
                          char const * = "",
                          char const * = "",
                          char const * = "");

  void and_provide(unsigned brand,
                   Message,
                   MessageKeyNames);

private:
  bool _blocking;
  std::string _target;
  bool _message_matters = false;
  Message _m_out;
  bool _keys_matter = false;
  MessageKeyNames _k_out;

  Task & _task;

  void print();
};

class OpenReceiveBuilder {
public:
  OpenReceiveBuilder(bool blocking, Task &);

  void and_fail(SysResult result);

  void and_provide(unsigned brand,
                   Message,
                   MessageKeyNames);
private:
  bool _blocking;

  Task & _task;

  void print();
};

class MockTest : public ::testing::Test {
protected:
  MockTest(char const * child_path);

  void SetUp() override;
  void TearDown() override;

  enum class Blocking { yes, no };

  SendBuilder expect_send_to(std::string target, Blocking = Blocking::yes);

  CallBuilder expect_call_to(std::string target, Blocking = Blocking::yes);

  OpenReceiveBuilder expect_open_receive(Blocking = Blocking::yes);

  Task & get_task() { return _task; }

private:
  Task _task;
};

}  // namespace sim

#endif  // SIM_MOCK_H
