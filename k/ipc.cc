#include "k/ipc.h"

#include "etl/error/check.h"

#include "k/unprivileged.h"

namespace k {

SysResult ustore(Message * addr, Message const & value) {
  for (unsigned i = 0; i < config::n_message_data; ++i) {
    CHECK(ustore(&addr->data[i], value.data[i]));
  }
  return SysResult::success;
}

SysResultWith<Message> uload(Message const * addr) {
  Message m;
  for (unsigned i = 0; i < config::n_message_data; ++i) {
    m.data[i] = CHECK(uload(&addr->data[i]));
  }
  return {etl::error::right, m};
}

}  // namespace k
