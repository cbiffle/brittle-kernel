#include "k/null_object.h"

namespace k {

SysResult NullObject::deliver_from(Brand, Sender *) {
  return SysResult::bad_key;
}

}  // namespace k
