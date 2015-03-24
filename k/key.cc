#include "k/key.h"

#include "etl/assert.h"

#include "k/object.h"
#include "k/object_table.h"

namespace k {

SysResult Key::send(Message const * m) {
  lazy_revoke();

  return objects[_index].ptr->send(_brand, m);
}

void Key::lazy_revoke() {
  ETL_ASSERT(_index < config::n_objects);

  auto const & te = objects[_index];
  if (_generation[0] != te.generation[0]
      || _generation[1] != te.generation[1]
      || te.ptr == nullptr) {
    // Reset to the null entry
    _index = 0;
    _generation[0] = objects[0].generation[0];
    _generation[1] = objects[0].generation[1];
  }
}

}  // namespace k
