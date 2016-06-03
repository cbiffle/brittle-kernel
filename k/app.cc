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

static Context * first_context;

static void initialize_well_known_objects(RangePtr<ObjectTable::Entry> entries,
                                          Arena & arena) {
  (void) new(&entries[0]) NullObject{0};

  {
    auto o = new(&entries[1]) ObjectTable{0};
    set_object_table(o);
    o->set_entries(entries);
  }

  {
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

  // Set the priority of all architectural exceptions.
  scb.set_exception_priority(HwException::mem_manage_fault, p_fault);
  scb.set_exception_priority(HwException::bus_fault,        p_fault);
  scb.set_exception_priority(HwException::usage_fault,      p_fault);
  scb.set_exception_priority(HwException::sv_call,          p_svc);
  scb.set_exception_priority(HwException::debug_monitor,    p_svc);
  scb.set_exception_priority(HwException::pend_sv,          p_pendsv);
  scb.set_exception_priority(HwException::sys_tick,         p_irq);

  // Set the priority of all external interrupts that may potentially get
  // enabled.
  auto & app = get_app_info();
  for (unsigned i = 0; i < app.external_interrupt_count; ++i) {
    nvic.set_irq_priority(i, p_irq);
  }
}

/*******************************************************************************
 * Creation of the rest of the initial objects.
 */

/*
 * Creates Memory objects in the given range -- one contiguous span as normal
 * memory, followed by a contiguous span of Device memory.  The break between
 * the two is determined by 'memory_map_count' from AppInfo.
 */
static void create_memory_objects(RangePtr<ObjectTable::Entry> entries) {
  auto & app = get_app_info();
  auto map = app.memory_map;

  for (unsigned i = 0; i < entries.count(); ++i) {
    auto is_device = i >= app.memory_map_count;

    // TODO: check that this does not alias the kernel or reserved devs

    auto atts = is_device ? Memory::device_attribute_mask : 0;
    (void) new(&entries[i]) 
      Memory{0, map[i].base, map[i].end - map[i].base, atts};
  }
}

/*
 * Fills the given range with Slot objects.
 */
static void fill_extra_slots(RangePtr<ObjectTable::Entry> entries) {
  for (auto & ent : entries) (void) new(&ent) Slot{0};
}


/*******************************************************************************
 * Initialization of first context.
 */

static void prepare_first_context() {
  // Mise en place.
  auto & app = get_app_info();
  auto & ot = object_table();

  // Add memory grants.
  for (auto & grant : app.initial_task_grants) {
    // Derive our loop index.  Silly C++11, not providing this.
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
  // unprivileged stores (below).
  first_context->apply_to_mpu();

  // Set up registers, some of which live in unprivileged stack memory.  If
  // the initial SP given by the app is not within its initial task grants,
  // we'll assert here.
  {
    // Derive address of the exception stack frame.
    auto s = reinterpret_cast<StackRegisters *>(app.initial_task_sp) - 1;
    // Fill in the two registers we require (PSR and PC).
    bool success = ustore(&s->psr, 1 << 24)
                && ustore(&s->r15, app.initial_task_pc);
    ALWAYS_PANIC_UNLESS(success, "fault setting up initial stack");
    // Now prepare the Context to resume from that stack frame.
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

  // Shorthand for wordy ABI constant:
  constexpr auto wkoc = kabi::well_known_object_count;

  // Compute required size of Object Table.
  auto table_size =
    wkoc + app.memory_map_count + app.device_map_count + app.extra_slot_count;

  // Use the Arena to create the Object Table's entry array.
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

  // Set up the initial object zoo.
  initialize_well_known_objects(entries, arena);
  create_memory_objects(
      entries.slice(wkoc, table_size - app.extra_slot_count));
  fill_extra_slots(
      entries.slice(table_size - app.extra_slot_count, table_size));
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
