#ifndef DEMO_MEMORY_H
#define DEMO_MEMORY_H

#include <cstdint>

namespace memory {

void split(unsigned k, uint32_t pos, unsigned slot_key, unsigned top_key_out);

uint32_t peek(unsigned k, uint32_t offset);
void poke(unsigned k, uint32_t offset, uint32_t data);

}  // namespace memory

#endif  // DEMO_MEMORY_H
