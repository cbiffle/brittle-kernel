#ifndef K_REGISTERS_H
#define K_REGISTERS_H

/*
 * The layout of unprivileged program registers in memory.
 *
 * Registers fall into two categories, hardware-saved and kernel-saved.
 *
 * Hardware-saved registers are saved, by the hardware, onto the unprivileged
 * program's stack.  We leave them there, in the layout described by the
 * StackRegisters type below.
 *
 * Kernel-saved registers are saved, by the kernel, into a "save area" in the
 * Context object.  The layout of these registers is described by the
 * SavedRegisters type below.
 */

#include "etl/armv7m/exception_frame.h"

#include "common/abi_types.h"
#include "common/message.h"

namespace k {

/*
 * Hardware-saved registers, as stashed on the stack by the hardware when we
 * enter the kernel.  This is merely an alias for the ARMv7-M layout.
 */
using StackRegisters = etl::armv7m::ExceptionFrame;

/*
 * Kernel-saved registers, as saved by the kernel entry routines into kernel
 * space.
 */
union SavedRegisters {
  // Array access to the kernel-saved registers.
  uint32_t raw[9];

  // Handy named accessors, for when you really want a particular register.
  struct {
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;
    uint32_t basepri;
  } named;

  // Interpretation of the kernel-saved registers as arguments to / results
  // from a syscall.
  ReceivedMessage sys;

  SavedRegisters() : raw{} {}  // Zero on creation
};

// Some basic consistency checks of the SavedRegisters union.

static_assert(sizeof(static_cast<SavedRegisters *>(nullptr)->raw)
      == sizeof(static_cast<SavedRegisters *>(nullptr)->named),
        "named and raw register fields mismatched");

}  // namespace k

#endif  // K_REGISTERS_H
