#include "k/null_object.h"

namespace k {

SysResult NullObject::deliver_from(uint32_t, Sender *) {
  return SysResult::bad_key;
}

}  // namespace k
