#ifndef K_OBJECT_H
#define K_OBJECT_H

#include <stdint.h>

#include "k/sys_result.h"

namespace k {

struct Message;  // see: k/ipc.h

class Object {
public:
  virtual SysResult call(uint32_t brand,
                         Message const *,
                         Message *) = 0;
};

}  // namespace k

#endif  // K_OBJECT_H
