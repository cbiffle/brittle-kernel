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
#include "k/registers.h"
#include "k/reply_gate.h"
#include "k/range_ptr.h"
#include "k/scheduler.h"
#include "k/sys_tick.h"
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

static Context * first_context;

static void initialize_well_known_objects(
    RangePtr<ObjectTable::Entry> entries,
    Arena & arena) {
  new(&entries[0]) NullObject;

  {
    auto o = new(&entries[1]) ObjectTable;
    set_object_table(o);
    o->set_entries(entries);
  }

  {
    auto b = new(arena.allocate(kabi::context_size)) Context::Body;
    first_context = new(&entries[2]) Context{*b};
  }

  {
    auto b = new(arena.allocate(kabi::reply_gate_size)) ReplyGate::Body;
    auto o = new(&entries[3]) ReplyGate{*b};
    first_context->set_reply_gate(o);
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
 * Interpretation of the object map from AppInfo.
 */

static void create_app_objects(RangePtr<ObjectTable::Entry> entries,
                               Arena & arena) {
  auto & app = get_app_info();
  auto map = app.object_map;

  for (unsigned i = well_known_object_count;
       i < app.object_table_entry_count;
       ++i) {
    switch (static_cast<AppInfo::ObjectType>(*map++)) {
      case AppInfo::ObjectType::memory:
        {
          auto base = *map++;
          auto l2_half_size = *map++;

          auto maybe_range = P2Range::of(base, l2_half_size);
          ETL_ASSERT(maybe_range);

          // TODO: check that this does not alias the kernel or reserved devs

          (void) new(&entries[i]) Memory{maybe_range.ref()};
          break;
        }

      case AppInfo::ObjectType::context:
        {
          auto reply_gate_index = *map++;
          ETL_ASSERT(reply_gate_index < app.object_table_entry_count);

          auto b = new(arena.allocate(sizeof(Context::Body))) Context::Body;
          auto c = new(&entries[i]) Context{*b};
          c->set_reply_gate(&entries[reply_gate_index].as_object());
          break;
        }

      case AppInfo::ObjectType::gate:
        {
          auto b = new(arena.allocate(sizeof(Gate::Body))) Gate::Body;
          (void) new(&entries[i]) Gate{*b};
          break;
        }

      case AppInfo::ObjectType::interrupt:
        {
          auto irq = *map++;
          ETL_ASSERT(irq < app.external_interrupt_count);

          auto b = new(arena.allocate(sizeof(Interrupt::Body)))
            Interrupt::Body{irq};
          auto o = new(&entries[i]) Interrupt{*b};
          get_irq_redirection_table()[irq] = o;
          break;
        }

      case AppInfo::ObjectType::reply_gate:
        {
          auto b = new(arena.allocate(sizeof(ReplyGate::Body)))
            ReplyGate::Body;
          (void) new(&entries[i]) ReplyGate{*b};
          break;
        }

      case AppInfo::ObjectType::sys_tick:
        {
          auto b = new(arena.allocate(sizeof(SysTick::Body)))
            SysTick::Body{0};
          auto o = new(&entries[i]) SysTick{*b};
          set_sys_tick_redirector(o);
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
  auto & ot = object_table();

  // Add memory grants.
  for (auto & grant : app.initial_task_grants) {
    // Derive our loop index.
    auto i = &grant - &app.initial_task_grants[0];
    // Attempt to create a key.
    auto maybe_key = ot[grant.memory_index].make_key(grant.brand_lo);
    // Require success.
    ETL_ASSERT(maybe_key);
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
    ETL_ASSERT(success);
    first_context->set_stack(s);
  }

  // Provide initial authority.
  first_context->key(4) = ot.make_key(0).ref();

  // Make it runnable.
  first_context->make_runnable();
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

  ETL_ASSERT(app.abi_token == current_abi_token);

  // Create an Arena that can allocate RAM from the donated RAM region.
  Arena arena{{reinterpret_cast<uint8_t *>(app.donated_ram_begin),
               reinterpret_cast<uint8_t *>(app.donated_ram_end)}};

  arena.reset();

  // Use the Arena to create the object array.
  auto entries = 
    RangePtr<ObjectTable::Entry>{
      static_cast<ObjectTable::Entry *>(
          arena.allocate(
            sizeof(ObjectTable::Entry) * app.object_table_entry_count)),
      app.object_table_entry_count};

  // Create the IRQ redirection table and initialize it to nulls.
  set_irq_redirection_table({
      static_cast<Interrupt * *>(
          arena.allocate(
            sizeof(Interrupt *) * app.external_interrupt_count)),
      app.external_interrupt_count,
      });
  for (unsigned i = 0; i < app.external_interrupt_count; ++i) {
    get_irq_redirection_table()[i] = nullptr;
  }

  initialize_well_known_objects(entries, arena);
  create_app_objects(entries, arena);
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
