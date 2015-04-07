#ifndef K_TYPES_H
#define K_TYPES_H

#include <cstdint>

namespace k {

using Brand      = std::uint64_t;
using Generation = std::uint32_t;
using Priority   = std::uint32_t;
using TableIndex = std::uint32_t;
using Selector   = std::uint16_t;

}  // namespace k

#endif  // K_TYPES_H
