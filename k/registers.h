#ifndef K_REGISTERS_H
#define K_REGISTERS_H

#include "etl/armv7m/exception_frame.h"

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
  uint32_t raw[8];
};

}  // namespace k

#endif  // K_REGISTERS_H
