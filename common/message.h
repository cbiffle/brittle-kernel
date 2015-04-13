#ifndef COMMON_MESSAGE_H
#define COMMON_MESSAGE_H

#include <cstdint>

#include "abi_types.h"
#include "descriptor.h"
#include "exceptions.h"

struct Message {
  Descriptor d0;
  std::uint32_t d1;
  std::uint32_t d2;
  std::uint32_t d3;

  constexpr Message(Descriptor d0_ = Descriptor::zero(),
                    std::uint32_t d1_ = 0,
                    std::uint32_t d2_ = 0,
                    std::uint32_t d3_ = 0)
    : d0{d0_}, d1{d1_}, d2{d2_}, d3{d3_} {}

  static constexpr Message failure(Exception e,
                                   std::uint32_t d3 = 0) {
    return {
      Descriptor::zero().with_error(true),
      std::uint32_t(e),
      std::uint32_t(std::uint64_t(e) >> 32),
      d3,
    };
  }
};

struct ReceivedMessage {
  Message m;
  Brand brand;
};

#endif  // COMMON_MESSAGE_H
