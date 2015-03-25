#include "k/object_table.h"

#include "etl/error/check.h"

#include "k/context.h"
#include "k/ipc.h"
#include "k/unprivileged.h"

namespace k {

ObjectTable object_table;

SysResult ObjectTable::call(uint32_t brand,
                            Message const * arg,
                            Message * result) {
  switch (CHECK(uload(&arg->data[0]))) {
    case 0:  // Mint Arbitrary Key
      {
        auto d1 = CHECK(uload(&arg->data[1]));
        if (d1 >= config::n_objects) {
          return SysResult::bad_message;
        }
        
        current->key(0).fill(d1, CHECK(uload(&arg->data[2])));
        return SysResult::success;
      }
    
    default:
      return SysResult::bad_message;
  }
}

}  // namespace k
