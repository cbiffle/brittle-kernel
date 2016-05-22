#ifndef K_PANIC_H
#define K_PANIC_H

/*
 * PANIC and friends are used to catch "impossible" conditions within the
 * kernel, as might result from errors in the code, memory corruption, etc.
 *
 * They are *not* intended for checking any behavior of (unprivileged)
 * programs.  If the kernel is correct, programs should not be able to trigger
 * PANIC, except by being corrupted at boot.
 *
 * The PANIC_IF/PANIC_UNLESS macros can be "disarmed" by defining
 * DISABLE_KERNEL_CONSISTENCY_CHECKS.
 */

namespace k {

/*
 * Implementation of the PANIC macro.
 */
__attribute__((noreturn))
void panic(char const *);

}  // namespace k

/*
 * PANIC catches supposedly unreachable code paths.  It cannot be disabled,
 * because it may e.g. prevent reaching the end of a non-void function without
 * a return.
 */
#define PANIC(__reason) ::k::panic(__reason)

/*
 * Checks if a condition is true and panics if so.  This is not intended to be
 * disabled, so it can be used to e.g. validate application content at startup.
 */
#define ALWAYS_PANIC_IF(__condition, __reason) \
  ((__condition) ? PANIC(__reason) : void(0))

/*
 * Checks if a condition is true, and panics otherwise.  This is not intended to
 * be disabled, like ALWAYS_PANIC_IF, above.
 */
#define ALWAYS_PANIC_UNLESS(__condition, __reason) \
  ((__condition) ? void(0) : PANIC(__reason))

#ifndef DISABLE_KERNEL_CONSISTENCY_CHECKS

  #define PANIC_UNLESS ALWAYS_PANIC_UNLESS
  #define PANIC_IF ALWAYS_PANIC_IF

#else  // defined(DISABLE_KERNEL_CONSISTENCY_CHECKS)

  /*
   * We do not evaluate either expression when checks are disabled, even though
   * this exposes us to subtle behavioral changes in "fast" builds.  Test the
   * bits you ship!
   */

  #define PANIC_UNLESS(__c, __r) ((void) 0)
  #define PANIC_IF(__c, __r) ((void) 0)

#endif

#endif  // K_PANIC_H
