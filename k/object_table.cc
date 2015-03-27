#include "k/object_table.h"

#include "etl/error/check.h"

#include "k/context.h"
#include "k/ipc.h"
#include "k/unprivileged.h"

namespace k {

ObjectTable object_table;

SysResult ObjectTable::call(uint32_t brand,
                            Context * caller) {
  Message m = CHECK(caller->get_message());
  switch (m.data[0]) {
    case 0: return mint_key(brand, caller, m);
    case 1: return read_key(brand, caller, m);
    
    default:
      return SysResult::bad_message;
  }
}

SysResult ObjectTable::mint_key(uint32_t,
                                Context * caller,
                                Message const & args) {
  auto index = args.data[1];
  auto brand = args.data[2];

  if (index >= config::n_objects) {
    return SysResult::bad_message;
  }

  CHECK(caller->put_message({0,0,0,0}));
  caller->key(0).fill(index, brand);
  caller->nullify_exchanged_keys(1);
  return SysResult::success;
}

SysResult ObjectTable::read_key(uint32_t,
                                Context * caller,
                                Message const & args) {
  auto index = current->key(0).get_index();
  auto brand = current->key(0).get_brand();

  CHECK(caller->put_message({index, brand, 0, 0}));
  caller->nullify_exchanged_keys();
  return SysResult::success;
}

}  // namespace k
