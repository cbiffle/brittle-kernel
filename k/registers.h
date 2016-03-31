#ifndef K_REGISTERS_H
#define K_REGISTERS_H

#include "etl/armv7m/exception_frame.h"

#include "common/abi_types.h"
#include "common/message.h"

namespace k {

/*
 * Machine registers, as stashed on the application stack by kernel entry
 * routines.
 */
using StackRegisters = etl::armv7m::ExceptionFrame;

/*
 * Machine registers, as saved by the kernel entry routines into kernel space.
 */
union SavedRegisters {
  uint32_t raw[9];

  struct {
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;
    uint32_t basepri;
  } named;

  struct {
    Message m;
    Brand b;
  } sys;

  SavedRegisters() : raw{} {}
};

static_assert(sizeof(static_cast<SavedRegisters *>(nullptr)->raw)
      == sizeof(static_cast<SavedRegisters *>(nullptr)->named),
        "named and raw register fields mismatched");

}  // namespace k

#endif  // K_REGISTERS_H
