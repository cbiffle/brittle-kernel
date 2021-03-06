#include <cxxabi.h>
#include <cstdint>
#include <cstring>

void __cxxabiv1::__cxa_pure_virtual() {
  while (true);
}

void * memset(void * s, int c, size_t n) {
  // Write out enough single bytes to align the output pointer.
  auto p8 = static_cast<uint8_t *>(s);
  auto end = static_cast<uint8_t *>(s) + n;
  while (p8 != end && (reinterpret_cast<uintptr_t>(p8) & 3)) {
    *p8++ = uint8_t(c);
  }

  // Double-cast to dodge alignment check (we know it's aligned)
  auto p32 = static_cast<uint32_t *>(static_cast<void *>(p8));
  auto end32 = reinterpret_cast<uint32_t *>(
      reinterpret_cast<uintptr_t>(end) & ~3);
  // Begin writing word-sized chunks.
  while (p32 != end32) {
    *p32++ = uint32_t(c) * 0x01010101;
  }

  // Finish up using single bytes.
  p8 = reinterpret_cast<uint8_t *>(end32);
  while (p8 != end) {
    *p8++ = uint8_t(c);
  }

  return s;
}
