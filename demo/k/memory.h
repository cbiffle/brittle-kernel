#ifndef DEMO_MEMORY_H
#define DEMO_MEMORY_H

#include <cstdint>

namespace memory {

void split(unsigned k, uint32_t pos, unsigned slot_key, unsigned top_key_out);

enum class ObjectType : uint32_t {
  context = 0,
  gate = 1,
  interrupt = 3,  // TODO synchronize with TypeCode in kernel
};

void become(unsigned k, ObjectType, unsigned arg, unsigned arg_key = 0);

uint32_t peek(unsigned k, uint32_t offset);
void poke(unsigned k, uint32_t offset, uint32_t data);

}  // namespace memory

#endif  // DEMO_MEMORY_H
