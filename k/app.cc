#include "k/app.h"

#include "etl/mem/arena.h"

#include "etl/armv7m/exception_table.h"
#include "etl/armv7m/exceptions.h"
#include "etl/armv7m/nvic.h"
#include "etl/armv7m/registers.h"
#include "etl/armv7m/scb.h"

#include "common/app_info.h"
#include "common/abi_sizes.h"

#include "k/memory.h"
#include "k/context.h"
#include "k/gate.h"
#include "k/interrupt.h"
#include "k/irq_redirector.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/panic.h"
#include "k/registers.h"
#include "k/range_ptr.h"
#include "k/scheduler.h"
#include "k/slot.h"
#include "k/unprivileged.h"

using etl::armv7m::Byte;
using etl::armv7m::get_basepri;
using etl::armv7m::nvic;
using etl::armv7m::scb;
using etl::armv7m::set_basepri;

using HwException = etl::armv7m::Exception;

namespace k {

/*******************************************************************************
 * Utility bits.
 */

/*
 * Allocation failure strategy to plug into the ETL Arena, below.  This converts
 * allocation failures into kernel panics.
 */
struct PanicOnAllocationFailure {
  static void * allocation_failed() {
    PANIC("out of donated RAM");
  }
};

/*
 * We'll use the ETL Arena to allocate objects, using this shorthand for our
 * preferred flavor.
 */
using Arena = etl::mem::Arena<PanicOnAllocationFailure,
                              etl::mem::DoNotRequireDeallocation>;

/*
 * This symbol is exported by the linker script to mark the end of kernel ROM.
 * The application's info is expected to appear directly thereafter.
 */
extern "C" {
  extern uint32_t _kernel_rom_end;
}

/*
 * Accessor for the application info.
 */
static inline AppInfo const & get_app_info() {
  return *reinterpret_cast<AppInfo const *>(&_kernel_rom_end);
}


/*******************************************************************************
 * Well-known objects.
 */

static constexpr unsigned well_known_object_count = 4;

static Context * first_context;

static void initialize_well_known_objects(
    RangePtr<ObjectTable::Entry> entries,
    Arena & arena) {
  new(&entries[0]) NullObject{0};

  {
    auto o = new(&entries[1]) ObjectTable{0};
    set_object_table(o);
    o->set_entries(entries);
  }

  {
    (void) new(&entries[3]) Slot{0};  // TODO recover well-known slot

    auto b = new(arena.allocate(kabi::context_size)) Context::Body;
    first_context = new(&entries[2]) Context{0, *b};
  }
}


/*******************************************************************************
 * Interrupt priority initialization.
 */

static void initialize_irq_priorities() {
  // Discover the number of implemented priority bits.
  set_basepri(0xFF);
  auto actual_basepri = get_basepri();
  set_basepri(0);

  // The 'actual_basepri' value will have ones in some number of MSBs.  We can
  // compute the LSB of the implemented priority field through a bitwise trick:
  auto priority_lsb = (actual_basepri ^ 0xFF) + 1;

  // Compute the priorities we'll use.
  auto p_fault  = Byte(0),
       p_svc    = Byte(p_fault + priority_lsb),
       p_irq    = Byte(p_svc + priority_lsb),
       p_pendsv = Byte(p_irq + priority_lsb);

  scb.set_exception_priority(HwException::mem_manage_fault, p_fault);
  scb.set_exception_priority(HwException::bus_fault, p_fault);
  scb.set_exception_priority(HwException::usage_fault, p_fault);
  scb.set_exception_priority(HwException::sv_call, p_svc);
  scb.set_exception_priority(HwException::debug_monitor, p_svc);
  scb.set_exception_priority(HwException::pend_sv, p_pendsv);
  scb.set_exception_priority(HwException::sys_tick, p_irq);

  auto & app = get_app_info();
  for (unsigned i = 0; i < app.external_interrupt_count; ++i) {
    nvic.set_irq_priority(i, p_irq);
  }
}

/*******************************************************************************
 * Interpretation of the memory map from AppInfo.
 */

static void create_memory_objects(RangePtr<ObjectTable::Entry> entries) {
  auto & app = get_app_info();
  auto map = app.memory_map;

  for (unsigned i = 0; i < app.memory_map_count; ++i) {
    // Compute half of the region size, because we use that, and it fits in 32
    // bits.
    auto half_end = map[i].end == 0 ? (1u << 31) : (map[i].end >> 1);
    auto half_size = half_end - (map[i].base >> 1);

    // That better be a power of two (zero doesn't count).
    ALWAYS_PANIC_IF(
        (half_size == 0)  // empty or 4GiB region
        || ((half_size & (half_size - 1)) != 0),  // not power of two
        "bad region size");

    // Which power of two is it?
    unsigned l2_half_size;
    for (l2_half_size = 0; l2_half_size < 32; ++l2_half_size) {
      if (half_size == (1u << l2_half_size)) break;
    }

    auto maybe_range = P2Range::of(map[i].base, l2_half_size);
    ALWAYS_PANIC_UNLESS(maybe_range, "bad region");

    // TODO: check that this does not alias the kernel or reserved devs

    (void) new(&entries[well_known_object_count + i])
      Memory{0, maybe_range.ref()};
  }
}

static void fill_extra_slots(RangePtr<ObjectTable::Entry> entries,
                             unsigned first_index,
                             unsigned count) {
  for (unsigned i = 0; i < count; ++i) {
    (void) new(&entries[first_index + i]) Slot{0};
  }
}


/*******************************************************************************
 * Context initialization for first context.
 */

static void prepare_first_context() {
  auto & app = get_app_info();
  auto & ot = object_table();

  // Add memory grants.
  for (auto & grant : app.initial_task_grants) {
    // Derive our loop index.
    auto i = &grant - &app.initial_task_grants[0];
    // Attempt to create a key.
    auto maybe_key = ot[grant.memory_index].make_key(grant.brand);
    // Require success.
    ALWAYS_PANIC_UNLESS(maybe_key, "bad brand in memory grant");
    // Attempt to load the corresponding memory region.  If the named object is
    // not a Memory object it will be silently ignored later.
    first_context->memory_region(i) = maybe_key.ref();
  }

  // Go ahead and apply those grants so that we can access stack with
  // unprivileged stores.
  first_context->apply_to_mpu();

  // Set up registers, some of which live in unprivileged stack memory.  If
  // the initial SP given by the app is not within its initial task grants,
  // we'll assert here.
  {
    auto s = reinterpret_cast<StackRegisters *>(app.initial_task_sp) - 1;
    bool success = ustore(&s->psr, 1 << 24)
                && ustore(&s->r15, app.initial_task_pc);
    ALWAYS_PANIC_UNLESS(success, "fault setting up initial stack");
    first_context->set_stack(reinterpret_cast<uint32_t>(s));
  }

  // Provide initial authority.
  first_context->key(1) = ot.make_key(0).ref();

  // Make it runnable.
  first_context->make_runnable();
}


/*******************************************************************************
 * App initialization top-level.
 */

/*
 * Consume donated RAM for the fixed kernel objects, and fill in the object
 * table.
 */
static void initialize_app() {
  auto & app = get_app_info();

  ALWAYS_PANIC_UNLESS(app.abi_token == current_abi_token, "bad ABI token");

  // Create an Arena that can allocate RAM from the donated RAM region.
  Arena arena{{reinterpret_cast<uint8_t *>(app.donated_ram_begin),
               reinterpret_cast<uint8_t *>(app.donated_ram_end)}};

  arena.reset();

  auto table_size = 4 + app.memory_map_count + app.extra_slot_count;

  // Use the Arena to create the object array.
  auto entries = 
    RangePtr<ObjectTable::Entry>{
      static_cast<ObjectTable::Entry *>(
          arena.allocate(sizeof(ObjectTable::Entry) * table_size)),
      table_size};

  // Create the IRQ redirection table and initialize it to nulls.
  auto irq_count = app.external_interrupt_count + 1;  // for SysTick
  set_irq_redirection_table({
      static_cast<Interrupt * *>(
          arena.allocate(
            sizeof(Interrupt *) * irq_count)),
      irq_count,
      });
  for (unsigned i = 0; i < irq_count; ++i) {
    get_irq_redirection_table()[i] = nullptr;
  }

  initialize_well_known_objects(entries, arena);
  create_memory_objects(entries);
  fill_extra_slots(entries, 4 + app.memory_map_count, app.extra_slot_count);
  prepare_first_context();
}


/*******************************************************************************
 * Scheduler startup.
 */

__attribute__((noreturn))
static void start_scheduler() {
  // Designate our first context as the scheduler's 'current' context.
  current = first_context;
  current->apply_to_mpu();

  asm volatile (
      "msr MSP, %0\n"       // Reset main stack pointer.
      "svc #0"
      :: "r"(&etl_armv7m_initial_stack_top)
      );
  __builtin_unreachable();
}

void start_app() {
  initialize_irq_priorities();
  initialize_app();
  start_scheduler();
}

}  // namespace k
