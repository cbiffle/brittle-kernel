#ifndef SYS_API_H
#define SYS_API_H

#include <cstdint>

namespace sys {

/*
 * Installs a Memory key, taken from 'mem_key', into the calling Context's MPU
 * region register 'index'.
 */
void map_memory(unsigned sys_key, unsigned mem_key, unsigned index);

/*
 * Voluntarily ends the calling process, delivering its arguments to the system
 * by way of explanation.
 */
enum class ExitReason : uint32_t {
  // The process has completed its work.
  finished = 0,
  // An internal assertion failed.
  assert_failed = 1,
};

__attribute__((noreturn))
void exit(unsigned sys_key, ExitReason reason,
                            uint32_t d1 = 0,
                            uint32_t d2 = 0,
                            uint32_t d3 = 0,
                            uint32_t d4 = 0);

}  // namespace sys

#endif  // SYS_API_H
