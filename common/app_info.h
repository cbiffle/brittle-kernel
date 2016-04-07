#ifndef COMMON_APP_INFO_H
#define COMMON_APP_INFO_H

#include <cstdint>

#include "abi_types.h"

/*
 * Describes the application's kernel interface.  An instance of this should be
 * placed in ROM directly after the end of the kernel's ROM image; the kernel
 * will interpret it to start the application.
 *
 * All the fields here are integers for simplicity, but many represent punned
 * pointers.
 */
struct AppInfo {
  // Number of object table entries.  Must be at least 4 for the kernel's
  // well-known objects.  Any objects beyond 4 are described by the object_map
  // at the end of the block.
  uint32_t object_table_entry_count;

  // Number of external interrupts that may be handled.  This determines the
  // size of the redirector table used to convert interrupts into messages.
  uint32_t external_interrupt_count;

  // Address of the start of RAM that the application is donating to the kernel
  // to create the objects described here.
  uint32_t donated_ram_begin;
  // Address of the end of donated RAM.
  uint32_t donated_ram_end;

  // Stack pointer for the application's initial task.  This must point inside
  // one of the grants described below, or initialization will fail.
  uint32_t initial_task_sp;

  // Program counter for the application's initial task.  This must point
  // inside one of the grants described below, or the application will fail to
  // start.
  uint32_t initial_task_pc;

  // Structure of a memory grant.
  struct MemGrant {
    // Object table index of the AddressRange giving authority for this grant.
    // If zero, the grant will be ignored.
    uint32_t address_range_index;
    // Top 32 bits of the AddressRange key to mint, which gives the MPU RASR
    // word for this region.
    uint32_t brand_hi;
    // Bottom 32 bits of the AddressRange key to mint, which gives the MPU
    // RBAR word for this region.
    uint32_t brand_lo;
  };
  // Table of memory grants for initial task.  (The number 4 here is arbitrary.)
  MemGrant initial_task_grants[4];

  // The object map.  This describes kernel objects needed by the application.
  // Each entry is a sequence of 32-bit words; the first is an ObjectType value
  // (below), possibly followed by type-specific parameter words.
  uint32_t object_map[];

  enum class ObjectType : uint32_t {
    // Describes a kernel AddressRange object.  Parameters:
    // - Begin address.
    // - End address.
    // - Prevent execution (if nonzero).
    // - Read-write (0), unprivileged read-only (1), read-only (2).
    address_range = 0,

    // Describes a kernel Context object.  Parameters:
    // - Object table index of reply gate.
    context,

    // Describes a kernel Gate object.  No parameters.
    gate,

    // Describes a kernel Interrupt object.  Parameters:
    // - IRQ number.
    interrupt,

    // Describes a kernel ReplyGate object.  No parameters.
    reply_gate,

    // Describes a kernel SysTick object.  No parameters.
    sys_tick,
  };
};

#endif  // COMMON_APP_INFO_H
