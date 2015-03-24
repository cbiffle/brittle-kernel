#include "k/null_object.h"

namespace k {

SysResult NullObject::send(uint32_t, Message const *) {
  return SysResult::bad_key;
}

}  // namespace k
