#include "k/object_table.h"

#include "k/context.h"
#include "k/ipc.h"

namespace k {

ObjectTable object_table;

SysResult ObjectTable::call(uint32_t brand,
                            Message const * arg,
                            Message * result) {
  // TODO unprivileged access
  switch (arg->data[0]) {
    case 0:  // Mint Arbitrary Key
      {
        if (arg->data[1] >= config::n_objects) {
          return SysResult::bad_message;
        }
        
        current->key(0).fill(arg->data[1], arg->data[2]);
        return SysResult::success;
      }
    
    default:
      return SysResult::bad_message;
  }
}

}  // namespace k
