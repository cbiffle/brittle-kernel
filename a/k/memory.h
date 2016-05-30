#ifndef A_K_MEMORY_H
#define A_K_MEMORY_H

#include <cstdint>
#include <cstddef>

#include "a/rt/keys.h"

namespace memory {

struct Region {
  uint32_t rbar;
  uint32_t rasr;

  constexpr size_t get_l2_half_size() const {
    return (rasr >> 1) & 0x1F;
  }

  constexpr size_t get_size_words() const {
    return 1u << (get_l2_half_size() - 1);
  }

  constexpr uintptr_t get_base() const {
    return rbar & ~0x1F;
  }

  constexpr bool contains(uintptr_t addr) const {
    return (addr - get_base() + 1) / 2 < (1u << get_l2_half_size());
  }
};

Region inspect(unsigned k);

rt::AutoKey split(unsigned k, uint32_t pos, unsigned slot_key);

enum class ObjectType : uint32_t {
  context = 0,
  gate = 1,
  interrupt = 3,  // TODO synchronize with TypeCode in kernel
};

void become(unsigned k, ObjectType, unsigned arg, unsigned arg_key = 0);

uint32_t peek(unsigned k, uint32_t offset);
void poke(unsigned k, uint32_t offset, uint32_t data);

void make_child(unsigned k, uintptr_t base, size_t size, unsigned slot_key);

}  // namespace memory

#endif  // A_K_MEMORY_H
