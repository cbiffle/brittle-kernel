#ifndef SYS_API_H
#define SYS_API_H

namespace sys {

/*
 * Installs a Memory key, taken from 'mem_key', into the calling Context's MPU
 * region register 'index'.
 */
void map_memory(unsigned sys_key, unsigned mem_key, unsigned index);

}  // namespace sys

#endif  // SYS_API_H
