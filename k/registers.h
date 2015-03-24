#ifndef K_REGISTERS_H
#define K_REGISTERS_H

#include "etl/armv7m/exception_frame.h"

namespace k {

/*
 * Machine registers, as stashed on the application stack by kernel entry
 * routines.
 */
struct Registers {
  using Word = etl::armv7m::Word;

  Word r4, r5, r6, r7;
  Word r8, r9, r10, r11;

  etl::armv7m::ExceptionFrame ef;
};

}  // namespace k

#endif  // K_REGISTERS_H
