#include "k/become.h"

#include "etl/destroy.h"
#include "etl/armv7m/mpu.h"

#include "common/abi_sizes.h"
#include "common/message.h"
#include "common/descriptor.h"

#include "k/context.h"
#include "k/gate.h"
#include "k/interrupt.h"
#include "k/memory.h"
#include "k/object_table.h"
#include "k/region.h"
#include "k/reply_sender.h"
#include "k/scheduler.h"
#include "k/sender.h"
#include "k/slot.h"

using etl::armv7m::Mpu;

namespace k {

enum class TypeCode {
  context = 0,
  gate = 1,
  interrupt = 2,
};

static unsigned size_for_type_code(TypeCode tc) {
  switch (tc) {
    case TypeCode::context:   return kabi::context_size;
    case TypeCode::gate:      return kabi::gate_size;
    case TypeCode::interrupt: return kabi::interrupt_size;

    // Other values are supposed to have been filtered out before this point.
    default: PANIC("become TC validation fail");
  }
}

void become(Memory & memory,
            Message const & m,
            Keys & k,
            ReplySender & reply_sender) {
  if (memory.is_device() || memory.child_count() || memory.parent()
      || !memory.is_mappable()) {
    // Can't transmogrify, this is wrong.
    // TODO mappability requirement is temporary
    reply_sender.message() = Message::failure(Exception::bad_operation);
    return;
  }

  if (m.d0 > uint32_t(TypeCode::interrupt)) {
    // Can't transmogrify, target object type not recognized.
    reply_sender.message() = Message::failure(Exception::bad_argument);
    return;
  }
  auto type_code = static_cast<TypeCode>(m.d0);

  if (size_for_type_code(type_code) > memory.get_size()) {
    // Can't transmogrify, memory too small.
    reply_sender.message() = Message::failure(Exception::bad_operation);
    return;
  }

  auto new_generation = memory.get_generation() + 1;

  // At this point it becomes dangerous (or at least suspicious) to use
  // 'memory' in any way, since we're going to start rewriting it shortly.

  auto bodymem = reinterpret_cast<void *>(memory.get_base());
  Object * newobj;

  etl::destroy(memory);

  switch (type_code) {
    case TypeCode::context:
      {
        auto b = new(bodymem) Context::Body;
        newobj = new(&memory) Context{new_generation, *b};
        break;
      }
    case TypeCode::gate:
      {
        auto b = new(bodymem) Gate::Body;
        newobj = new(&memory) Gate{new_generation, *b};
        break;
      }
    case TypeCode::interrupt:
      {
        auto b = new(bodymem) Interrupt::Body{m.d1};
        newobj = new(&memory) Interrupt{new_generation, *b};
        break;
      }
  }
  // Provide a key to the new object.
  reply_sender.set_key(1, newobj->make_key(0).ref());  // TODO brand?

  // Update MPU, in case the transmogrified object was in the current Context's
  // memory map.
  current->apply_to_mpu();
}

}  // namespace k
