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

void Task::wait_for_halt() {
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

RequestType Task::next_nontrivial_syscall() {
  while (true) {
    auto rt = in<RequestType>();
    switch (rt) {
      case RequestType::copy_cap:
        {
          auto r = in<CopyCapRequest>();
          set_key(r.to_index, get_key(r.from_index));
          set_key(r.from_index, "");  // TODO
          break;
        }

      case RequestType::null_cap:
        {
          auto r = in<NullCapRequest>();
          set_key(r.index, "");
          break;
        }

      case RequestType::mask_port:
        {
          auto r = in<MaskPortRequest>();
          _masked_ports.insert(r.index);
          break;
        }

      case RequestType::unmask_port:
        {
          auto r = in<UnmaskPortRequest>();
          _masked_ports.erase(r.index);
          break;
        }

      case RequestType::send:
      case RequestType::call:
      case RequestType::open_receive:
        return rt;
    }

    out(ResponseType::complete);
    out(CompleteResponse { SysResult::success });
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

void Task::revoke_key(std::string const & k) {
  for (unsigned i = 0; i < n_keys_sent; ++i) {
    if (get_key(i) == k) {
      set_key(i, "<REVOKED>");
    }
  }

}

bool Task::is_port_masked(uintptr_t p) {
  return _masked_ports.find(p) != _masked_ports.end();
}

}  // namespace sim
