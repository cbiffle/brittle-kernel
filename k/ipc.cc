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

}  // namespace k
