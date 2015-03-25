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
    case 0: return mint_key(brand, arg, result);
    case 1: return mint_key(brand, arg, result);
    
    default:
      return SysResult::bad_message;
  }
}

SysResult ObjectTable::mint_key(uint32_t,
                                Message const * arg,
                                Message * result) {
  auto d1 = CHECK(uload(&arg->data[1]));
  if (d1 >= config::n_objects) {
    return SysResult::bad_message;
  }

  current->key(0).fill(d1, CHECK(uload(&arg->data[2])));
  return SysResult::success;
}

SysResult ObjectTable::read_key(uint32_t,
                                Message const * arg,
                                Message * result) {
  auto index = current->key(0).get_index();
  auto brand = current->key(0).get_brand();

  current->nullify_exchanged_keys();

  ustore(result, {index, brand, 0, 0});
}

}  // namespace k
