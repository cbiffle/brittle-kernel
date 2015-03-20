#ifndef SIM_MOCK_H
#define SIM_MOCK_H

#include <map>
#include <memory>
#include <set>

#include <gtest/gtest.h>

#include "protocol.h"

namespace mock {

static constexpr unsigned n_keys_sent = 4;

struct MessageKeyNames {
  std::string name[n_keys_sent];
};

inline bool operator==(MessageKeyNames const & a, MessageKeyNames const & b) {
  for (unsigned i = 0; i < n_keys_sent; ++i) {
    if (a.name[i] != b.name[i]) return false;
  }

  return true;
}

inline bool operator!=(MessageKeyNames const & a, MessageKeyNames const & b) {
  return !(a == b);
}


class Task {
public:
  Task(char const * child_path);

  void start();
  void stop();

  std::string get_key(uintptr_t);
  void set_key(uintptr_t, std::string);
  MessageKeyNames get_pass_keys();
  void set_pass_keys(MessageKeyNames const &);
  void clear_pass_keys();
  void revoke_key(std::string const &);
  bool is_port_masked(uintptr_t);

  void wait_for_syscall();

  template <typename T>
  T in() {
    T tmp;
    if (read(_in, &tmp, sizeof(tmp)) != sizeof(tmp)) {
      perror("reading from task");
      throw std::logic_error("I/O");
    }
    return tmp;
  }

  template <typename T>
  void out(T const & data) {
    if (write(_out, &data, sizeof(data)) != sizeof(data)) {
      perror("writing to task");
      throw std::logic_error("I/O");
    }
  }

  void sim_sys(SendRequest const &);
  void sim_sys(CallRequest const &);

private:
  char const * _child_path;
  int _in;
  int _out;
  pid_t _child;
  std::map<uintptr_t, std::string> _clist;
  std::set<uintptr_t> _masked_ports;
};


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


/*******************************************************************************
 * Actual test
 */

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

}  // namespace mock

#endif  // SIM_MOCK_H
