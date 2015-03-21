#ifndef SIM_TASK_H
#define SIM_TASK_H

#include <map>
#include <memory>
#include <set>

#include "config.h"
#include "protocol.h"

namespace sim {

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

  void wait_for_halt();
  RequestType next_nontrivial_syscall();

private:
  char const * _child_path;
  int _in;
  int _out;
  pid_t _child;
  std::map<uintptr_t, std::string> _clist;
  std::set<uintptr_t> _masked_ports;
};

}  // namespace sim

#endif  // SIM_TASK_H
