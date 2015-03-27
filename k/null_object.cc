#include "k/null_object.h"

namespace k {

SysResult NullObject::call(uint32_t, Context *) {
  return SysResult::bad_key;
}

}  // namespace k
