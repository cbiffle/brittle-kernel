#include "k/app.h"

#include "etl/mem/arena.h"

#include "etl/armv7m/exception_table.h"

#include "common/app_info.h"

#include "k/address_range.h"
#include "k/context.h"
#include "k/gate.h"
#include "k/interrupt.h"
#include "k/irq_redirector.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/registers.h"
#include "k/reply_gate.h"
#include "k/range_ptr.h"
#include "k/scheduler.h"
#include "k/unprivileged.h"

namespace k {

/*******************************************************************************
 * Utility bits.
 */

/*
 * We'll use the ETL Arena to allocate objects, using this shorthand for our
 * preferred flavor.
 */
using Arena = etl::mem::Arena<etl::mem::AssertOnAllocationFailure,
                              etl::mem::DoNotRequireDeallocation>;

/*
 * This symbol is exported by the linker script to mark the end of kernel ROM.
 * The application's info is expected to appear directly thereafter.
 */
extern "C" {
  extern uint32_t _kernel_rom_end;
}

static inline AppInfo const & get_app_info() {
  return *reinterpret_cast<AppInfo const *>(&_kernel_rom_end);
}


/*******************************************************************************
 * Well-known objects.
 */

static constexpr unsigned well_known_object_count = 4;

// This fulfills the prototype from object_table.h
ObjectTable object_table;

static NullObject null_object;
static Context first_context;
static ReplyGate first_context_reply;


static void initialize_well_known_objects() {
  object_table[0].ptr = &null_object;
  null_object.set_index(0);

  object_table[1].ptr = &object_table;
  object_table.set_index(1);

  object_table[2].ptr = &first_context;
  first_context.set_index(2);

  object_table[3].ptr = &first_context_reply;
  first_context_reply.set_index(3);
  first_context.set_reply_gate_index(3);
}


/*******************************************************************************
 * Interpretation of the object map from AppInfo.
 */

static void create_app_objects(Arena & arena) {
  auto & app = get_app_info();
  auto map = app.object_map;

  for (unsigned i = well_known_object_count;
       i < app.object_table_entry_count;
       ++i) {
    switch (static_cast<AppInfo::ObjectType>(*map++)) {
      case AppInfo::ObjectType::address_range:
        {
          auto begin = reinterpret_cast<uint8_t *>(*map++);
          auto end   = reinterpret_cast<uint8_t *>(*map++);
          auto prevent_execution = !!*map++;
          auto read_only =
            static_cast<AddressRange::ReadOnly>(*map++);
          auto range = RangePtr<uint8_t>{begin, end};

          // TODO: check that this does not alias the kernel or reserved devs

          auto ar = new(arena.allocate(sizeof(AddressRange)))
            AddressRange{range, prevent_execution, read_only};
          object_table[i].ptr = ar;
          ar->set_index(i);
          break;
        }

      case AppInfo::ObjectType::context:
        {
          auto reply_gate_index = *map++;
          ETL_ASSERT(reply_gate_index < app.object_table_entry_count);

          auto c = new(arena.allocate(sizeof(Context))) Context;
          object_table[i].ptr = c;
          c->set_index(i);
          c->set_reply_gate_index(reply_gate_index);
          break;
        }

      case AppInfo::ObjectType::gate:
        {
          auto o = new(arena.allocate(sizeof(Gate))) Gate;
          object_table[i].ptr = o;
          o->set_index(i);
          break;
        }

      case AppInfo::ObjectType::interrupt:
        {
          auto irq = *map++;
          auto o = new(arena.allocate(sizeof(Interrupt))) Interrupt{irq};
          object_table[i].ptr = o;
          o->set_index(i);
          get_irq_redirection_table()[irq] = o;
          break;
        }

      case AppInfo::ObjectType::reply_gate:
        {
          auto o = new(arena.allocate(sizeof(ReplyGate))) ReplyGate;
          object_table[i].ptr = o;
          o->set_index(i);
          break;
        }

      default:
        ETL_ASSERT(false);
    }
  }
}


/*******************************************************************************
 * Context initialization for first context.
 */

static void prepare_first_context() {
  auto & app = get_app_info();

  // Add memory grants.
  for (auto & grant : app.initial_task_grants) {
    // Derive our loop index.
    auto i = &grant - &app.initial_task_grants[0];
    // Reconstruct the brand from parts.
    auto brand = (Brand(grant.brand_hi) << 32) | grant.brand_lo;
    // Attempt to create a key.
    auto maybe_key =
      object_table[grant.address_range_index].ptr->make_key(brand);
    // Attempt to load the corresponding memory region.  If key creation failed
    // the Maybe will assert here.  If the named object is not an AddressRange
    // it will be silently ignored later.
    first_context.memory_region(i) = maybe_key.ref();
  }

  // Go ahead and apply those grants so that we can access stack with
  // unprivileged stores.
  first_context.apply_to_mpu();

  // Set up registers, some of which live in unprivileged stack memory.  If
  // the initial SP given by the app is not within its initial task grants,
  // we'll assert here.
  {
    auto s = reinterpret_cast<StackRegisters *>(app.initial_task_sp) - 1;
    bool success = ustore(&s->psr, 1 << 24)
                && ustore(&s->r15, app.initial_task_pc);
    ETL_ASSERT(success);
    first_context.set_stack(s);
  }

  // Provide initial authority.
  first_context.key(4) = object_table.make_key(0).ref();

  // Make it runnable.
  first_context.make_runnable();
}


/*******************************************************************************
 * App initialization top-level.
 */

/*
 * Assign donated RAM to all the kernel objects requested by the application,
 * and initialize them.
 */
static void initialize_app() {
  auto & app = get_app_info();

  // Create an Arena that can allocate RAM from the donated RAM region.
  Arena arena{{reinterpret_cast<uint8_t *>(app.donated_ram_begin),
               reinterpret_cast<uint8_t *>(app.donated_ram_end)}};

  arena.reset();

  // Use the Arena to create the entry array for the object table itself.
  object_table.set_entries({
      static_cast<ObjectTable::Entry *>(
          arena.allocate(
            sizeof(ObjectTable::Entry) * app.object_table_entry_count)),
      app.object_table_entry_count,
      });
  for (unsigned i = 0; i < app.object_table_entry_count; ++i) {
    object_table[i] = {0, nullptr};
  }

  set_irq_redirection_table({
      static_cast<Interrupt * *>(
          arena.allocate(
            sizeof(Interrupt *) * app.external_interrupt_count)),
      app.external_interrupt_count,
      });
  for (unsigned i = 0; i < app.external_interrupt_count; ++i) {
    get_irq_redirection_table()[i] = nullptr;
  }

  initialize_well_known_objects();
  create_app_objects(arena);
  prepare_first_context();
}


/*******************************************************************************
 * Scheduler startup.
 */

__attribute__((noreturn))
static void start_scheduler() {
  // Designate our first context as the scheduler's 'current' context.
  current = &first_context;
  current->apply_to_mpu();

  asm volatile (
      "mov r0, sp\n"        // Get current stack pointer and
      "msr PSP, r0\n"       // replicate it in process stack pointer.
      "msr MSP, %0\n"       // Reset main stack pointer.
      "mov r0, #2\n"
      "msr CONTROL, r0\n"   // Drop privs, switch stacks.
      "svc #0"
      :: "r"(&etl_armv7m_initial_stack_top)
      );
  __builtin_unreachable();
}

void start_app() {
  initialize_app();
  start_scheduler();
}

}  // namespace k