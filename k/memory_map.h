#ifndef K_MEMORY_MAP_H
#define K_MEMORY_MAP_H

namespace k {

/*
 * Checks whether an unprivileged access to the memory area bounded by begin
 * (inclusive) and end (exclusive) would be potentially harmful to the kernel's
 * invariants.
 */
bool is_unprivileged_access_ok(void * begin, void * end);

}  // namespace k

#endif  // K_MEMORY_MAP_H
