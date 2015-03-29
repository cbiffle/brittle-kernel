#include "k/zoo.h"

#include "k/null_object.h"
#include "k/object_table.h"

namespace k {

Context contexts[config::n_contexts];

static NullObject null;

ObjectTable object_table;

void init_zoo() {
  // First, wire up the special objects.
  auto constexpr special_objects = 2;
  object_table[0].ptr = &null;
  null.set_index(0);

  object_table[1].ptr = &object_table;
  object_table.set_index(1);

  // Then, wire up the contexts.
  static_assert(config::n_objects >= config::n_contexts + special_objects,
                "Not enough object table entries for objects in zoo.");
  for (unsigned i = 0; i < config::n_contexts; ++i) {
    object_table[i + special_objects].ptr = &contexts[i];
    contexts[i].set_index(i + special_objects);
  }
}

}  // namespace k
