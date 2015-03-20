#include "task.h"

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

#include "protocol.h"

namespace sim {

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

bool Task::sim_sys(SendRequest const & r) {
  switch (r.m.data[0]) {
    case 0:  // move cap
      set_key(r.m.data[2], get_key(r.m.data[1]));
      set_key(r.m.data[1], "");

      out(ResponseType::complete);
      out(CompleteResponse { SysResult::success });
      return true;

    case 1:  // mask port
      _masked_ports.insert(r.m.data[1]);
      out(ResponseType::complete);
      out(CompleteResponse { SysResult::success });
      return true;

    case 2:  // unmask port
      _masked_ports.erase(r.m.data[1]);
      out(ResponseType::complete);
      out(CompleteResponse { SysResult::success });
      return true;

    default:
      return false;
  }
}

void Task::revoke_key(std::string const & k) {
  for (unsigned i = 0; i < n_keys_sent; ++i) {
    if (get_key(i) == k) {
      set_key(i, "<REVOKED>");
    }
  }

}

bool Task::sim_sys(CallRequest const & r) {
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
      return true;

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
      return true;

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
      return true;

    default:
      return false;
  }
}

bool Task::is_port_masked(uintptr_t p) {
  return _masked_ports.find(p) != _masked_ports.end();
}

}  // namespace sim
