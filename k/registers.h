#ifndef K_REGISTERS_H
#define K_REGISTERS_H

#include "etl/armv7m/exception_frame.h"

namespace k {

/*
 * Machine registers, as stashed on the application stack by kernel entry
 * routines.
 */
using Registers = etl::armv7m::ExceptionFrame;

}  // namespace k

#endif  // K_REGISTERS_H
