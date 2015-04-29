#ifndef K_UNPRIVILEGED_H
#define K_UNPRIVILEGED_H

/*
 * Access memory using the currently loaded MPU configuration, as though the
 * access were taking place from an unprivileged task.  This can be used to
 * safely dereference pointers (including the contents of registers) read from
 * tasks as arguments or for state inspection.
 */

#include <stdint.h>

#include "etl/armv7m/types.h"

#include "k/maybe.h"

namespace k {

extern uintptr_t mm_fault_recovery_handler;

/*
 * The uload family of functions dereference the given pointer if the task
 * currently loaded on the MPU would be able to do so.  Otherwise, they
 * return SysResult::fault.
 */
__attribute__((warn_unused_result))
Maybe<etl::armv7m::Word> uload(etl::armv7m::Word const *);
__attribute__((warn_unused_result))
Maybe<etl::armv7m::Byte> uload(etl::armv7m::Byte const *);

/*
 * The ustore family of functions write to the given pointer if the task
 * currently loaded on the MPU would be able to do so.  On success, they
 * return SysResult::success.  Otherwise, they return SysResult::fault.
 */
__attribute__((warn_unused_result))
bool ustore(etl::armv7m::Word *, etl::armv7m::Word);
__attribute__((warn_unused_result))
bool ustore(etl::armv7m::Byte *, etl::armv7m::Byte);

__attribute__((warn_unused_result))
bool ustore(etl::armv7m::DoubleWord *, etl::armv7m::DoubleWord);

}  // namespace k

#endif  // K_UNPRIVILEGED_H
