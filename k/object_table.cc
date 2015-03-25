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
  auto index = CHECK(uload(&arg->data[1]));
  auto brand = CHECK(uload(&arg->data[2]));

  if (index >= config::n_objects) {
    return SysResult::bad_message;
  }

  current->nullify_exchanged_keys();
  current->key(0).fill(index, brand);
  return SysResult::success;
}

SysResult ObjectTable::read_key(uint32_t,
                                Message const * arg,
                                Message * result) {
  auto index = current->key(0).get_index();
  auto brand = current->key(0).get_brand();

  CHECK(ustore(result, {index, brand, 0, 0}));
  current->nullify_exchanged_keys();
  return SysResult::success;
}

}  // namespace k
