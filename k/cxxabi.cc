#include <cxxabi.h>
#include <cstdint>
#include <cstring>

void __cxxabiv1::__cxa_pure_virtual() {
  while (true);
}

void * memset(void * s, int c, size_t n) {
  auto s8 = static_cast<uint8_t *>(s);
  for (size_t i = 0; i < n; ++i) {
    s8[i] = uint8_t(c);
  }
  return s;
}
