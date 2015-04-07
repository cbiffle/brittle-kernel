#ifndef K_IPC_H
#define K_IPC_H

#include <stdint.h>

#include "k/config.h"
#include "k/exceptions.h"
#include "k/sys_result.h"
#include "k/types.h"

namespace k {

class Descriptor {
public:
  constexpr inline Descriptor() : _bits(0) {}
  static constexpr Descriptor zero() { return {}; }
  static constexpr Descriptor from_bits(uint32_t bits) { return {bits}; }

  constexpr Descriptor with_selector(Selector s) const {
    return {(_bits & ~0xFFFF) | (s & 0xFFFF)};
  }

  constexpr Selector get_selector() const {
    return _bits & 0xFFFF;
  }

  constexpr Descriptor with_error(bool e) const {
    return {(_bits & ~(1 << 16)) | (uint32_t(e) << 16)};
  }

  constexpr bool get_error() const {
    return _bits & (1 << 16);
  }

  constexpr Descriptor with_send_enabled(bool e) const {
    return {(_bits & ~(1 << 17)) | (uint32_t(e) << 17)};
  }

  constexpr bool get_send_enabled() const {
    return _bits & (1 << 17);
  }

  constexpr Descriptor with_receive_enabled(bool e) const {
    return {(_bits & ~(1 << 18)) | (uint32_t(e) << 18)};
  }

  constexpr bool get_receive_enabled() const {
    return _bits & (1 << 18);
  }

  constexpr Descriptor with_block(bool e) const {
    return {(_bits & ~(1 << 19)) | (uint32_t(e) << 19)};
  }

  constexpr bool get_block() const {
    return _bits & (1 << 19);
  }

  constexpr Descriptor with_target(unsigned t) const {
    return {(_bits & ~(0xF << 20)) | ((t & 0xF) << 20)};
  }

  constexpr unsigned get_target() const {
    return (_bits >> 20) & 0xF;
  }

  constexpr Descriptor with_source(unsigned s) const {
    return {(_bits & ~(0xF << 24)) | ((s & 0xF) << 24)};
  }

  constexpr unsigned get_source() const {
    return (_bits >> 24) & 0xF;
  }

  constexpr Descriptor with_sysnum(unsigned s) const {
    return {(_bits & ~(0xF << 28)) | ((s & 0xF) << 28)};
  }

  constexpr unsigned get_sysnum() const {
    return (_bits >> 28) & 0xF;
  }


  constexpr Descriptor sanitized() const {
    return zero().with_selector(get_selector())
                 .with_error(get_error());
  }

  constexpr explicit operator uint32_t() const {
    return _bits;
  }

  constexpr bool is_call() const {
    return get_receive_enabled() && get_source() == 0;
  }

private:
  constexpr inline Descriptor(uint32_t bits) : _bits(bits) {}
  uint32_t _bits;
};

struct Message {
  Descriptor d0;
  uint32_t d1;
  uint32_t d2;
  uint32_t d3;

  constexpr Message(Descriptor d0_ = Descriptor::zero(),
                    uint32_t d1_ = 0,
                    uint32_t d2_ = 0,
                    uint32_t d3_ = 0)
    : d0{d0_}, d1{d1_}, d2{d2_}, d3{d3_} {}

  static constexpr Message failure(Exception e,
                                   uint32_t d3 = 0) {
    return {
      Descriptor::zero().with_error(true),
      uint32_t(e),
      uint32_t(uint64_t(e) >> 32),
      d3,
    };
  }
};

struct ReceivedMessage {
  Message m;
  Brand brand;
};

}  // namespace k

#endif  // K_IPC_H
