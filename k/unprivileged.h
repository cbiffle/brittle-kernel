#ifndef K_UNPRIVILEGED_H
#define K_UNPRIVILEGED_H

/*
 * Operations that access memory from privileged code, but using the currently
 * loaded MPU configuration, as though the access were taking place from an
 * unprivileged context.  This can be used to safely dereference pointers
 * into unprivileged memory, without potentially leaking or mutating kernel
 * state.
 *
 * Note that the kernel is designed to use these operations *very* *rarely.*
 * They only come into play during initialization and for inspecting
 * unprivileged stacks.
 */

#include <cstdint>

#include "etl/armv7m/types.h"

#include "k/maybe.h"

namespace k {

/*
 * Variable used to communicate with the memory management fault handler.
 */
extern uintptr_t mm_fault_recovery_handler;

/*
 * The uload family of functions dereference the given pointer if the task
 * currently loaded on the MPU would be able to do so.  On success, they return
 * the value read; on failure, they return nothing.
 */
__attribute__((warn_unused_result))
Maybe<etl::armv7m::Word> uload(etl::armv7m::Word const *);
__attribute__((warn_unused_result))
Maybe<etl::armv7m::Byte> uload(etl::armv7m::Byte const *);

/*
 * The ustore family of functions write to the given pointer if the task
 * currently loaded on the MPU would be able to do so.  They return a flag
 * indicating success (true) or failure (false).
 */
__attribute__((warn_unused_result))
bool ustore(etl::armv7m::Word *, etl::armv7m::Word);
__attribute__((warn_unused_result))
bool ustore(etl::armv7m::Byte *, etl::armv7m::Byte);

}  // namespace k

#endif  // K_UNPRIVILEGED_H
