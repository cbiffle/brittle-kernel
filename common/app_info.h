#ifndef COMMON_APP_INFO_H
#define COMMON_APP_INFO_H

#include <cstdint>

#include "abi_types.h"

static constexpr uint32_t current_abi_token = 0xca11ab1e;

/*
 * Describes the application's kernel interface.  An instance of this should be
 * placed in ROM directly after the end of the kernel's ROM image; the kernel
 * will interpret it to start the application.
 *
 * All the fields here are integers for simplicity, but many represent punned
 * pointers.
 */
struct AppInfo {
  // ABI token.  The kernel checks this before attempting to interpret the rest
  // of the AppInfo.  If the ABI token is wrong, initialization fails.
  uint32_t abi_token;

  // Number of memory objects predefined by the memory map following this block.
  uint32_t memory_map_count;
  // Number of extra free object table slots desired.  The number of slots in
  // the object table will be given by:
  //   4 + memory_map_count + extra_slot_count;
  uint32_t extra_slot_count;

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
    // Object table index of the Memory object giving authority for this grant.
    // If zero, the grant will be ignored.  Otherwise, this needs to be between
    // 4 and 4 + memory_map_count.
    uint32_t memory_index;
    // Bottom 32 bits of the Memory key to mint, which gives the MPU RASR
    // attributes.
    uint32_t brand_lo;
  };
  // Table of memory grants for initial task.  (The number 4 here is arbitrary.)
  MemGrant initial_task_grants[4];

  // An entry in the Memory Map (below).
  struct MemoryMapEntry {
    // Base address of memory map entry.
    uint32_t base;
    // End address of memory map entry.
    uint32_t end;
  };

  // The memory map, giving the set of Memory Objects that should be
  // manufactured by the kernel before starting the initial context.
  MemoryMapEntry memory_map[];
};

#endif  // COMMON_APP_INFO_H
