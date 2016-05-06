#include "k/become.h"

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
  interrupt = 3,  // TODO renumber
};

static Maybe<TypeCode> extract_type_code(uint32_t arg) {
  if (arg < 5) {
    return static_cast<TypeCode>(arg);
  } else {
    return nothing;
  }
}

static unsigned l2_size_for_type_code(TypeCode tc) {
  switch (tc) {
    case TypeCode::context: return kabi::context_l2_size;
    case TypeCode::gate: return kabi::gate_l2_size;
    case TypeCode::interrupt: return kabi::interrupt_l2_size;
    default:
      return 0;
  }
}

void become(Memory & memory,
            Message const & m,
            Keys & k,
            ReplySender & reply_sender) {
  auto maybe_type_code = extract_type_code(m.d0);
  if (!maybe_type_code) {
    // Can't transmogrify, target object type not recognized.
    reply_sender.message() = Message::failure(Exception::bad_argument);
    return;
  }

  auto type_code = maybe_type_code.ref();
  auto range = memory.get_range();

  if (l2_size_for_type_code(type_code) != range.l2_size()) {
    // Can't transmogrify, size is wrong.
    reply_sender.message() = Message::failure(Exception::bad_operation);
    return;
  }

  auto new_generation = memory.get_generation() + 1;

  // At this point it becomes dangerous (or at least suspicious) to use
  // 'memory' in any way, since we're going to start rewriting it shortly.

  Object * newobj;
  auto bodymem = reinterpret_cast<void *>(range.base());

  switch (type_code) {
    case TypeCode::context:
      {
        memory.~Memory();

        auto b = new(bodymem) Context::Body;
        newobj = new(&memory) Context{new_generation, *b};
        break;
      }
    case TypeCode::gate:
      {
        memory.~Memory();

        auto b = new(bodymem) Gate::Body;
        newobj = new(&memory) Gate{new_generation, *b};
        break;
      }
    case TypeCode::interrupt:
      {
        memory.~Memory();

        auto b = new(bodymem)
          Interrupt::Body{m.d1};
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
