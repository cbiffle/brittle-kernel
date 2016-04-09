#ifndef COMMON_DESCRIPTOR_H
#define COMMON_DESCRIPTOR_H

/*
 * Value type representing an operation descriptor.  Descriptors are used with
 * all syscalls, but are most visible on IPC operations, where they form the
 * first word of the messages exchanged.
 *
 * This file is dotted with ETL_INLINE directives to deal with overly
 * conservative inlining heuristics in GCC 4.8.  The Descriptor type is
 * intended to be fully inlined.
 */

#include "etl/attribute_macros.h"

#include <cstdint>

#include "abi_types.h"

class Descriptor {
public:
  /*
   * A zero descriptor.
   */
  constexpr inline Descriptor() : _bits(0) {}
  static constexpr Descriptor zero() { return {}; }

  /*
   * Conversion from an integer.
   */
  static constexpr Descriptor from_bits(uint32_t bits) { return {bits}; }

  /*
   * Derives a new Descriptor with the selector replaced.
   */
  constexpr Descriptor with_selector(Selector s) const {
    return {(_bits & ~0xFFFF) | (s & 0xFFFF)};
  }

  /*
   * Extracts the selector.
   */
  constexpr Selector get_selector() const {
    return _bits & 0xFFFF;
  }

  /*
   * Derives a new Descriptor with the error bit replaced.
   */
  constexpr Descriptor with_error(bool e) const {
    return {(_bits & ~(1 << 16)) | (uint32_t(e) << 16)};
  }

  /*
   * Extracts the error bit.
   */
  constexpr bool get_error() const {
    return _bits & (1 << 16);
  }

  /*
   * Derives a new Descriptor with the send-phase-enable bit replaced.
   */
  constexpr Descriptor with_send_enabled(bool e) const {
    return {(_bits & ~(1 << 17)) | (uint32_t(e) << 17)};
  }

  /*
   * Extracts the send-phase-enable bit.
   */
  constexpr bool get_send_enabled() const {
    return _bits & (1 << 17);
  }

  /*
   * Derives a new Descriptor with the receive-phase-enable bit replaced.
   */
  constexpr Descriptor with_receive_enabled(bool e) const {
    return {(_bits & ~(1 << 18)) | (uint32_t(e) << 18)};
  }

  /*
   * Extracts the receive-phase-enable bit.
   */
  ETL_INLINE
  constexpr bool get_receive_enabled() const {
    return _bits & (1 << 18);
  }

  /*
   * Derives a new Descriptor with the blocking bit replaced.
   */
  constexpr Descriptor with_block(bool e) const {
    return {(_bits & ~(1 << 19)) | (uint32_t(e) << 19)};
  }

  /*
   * Extracts the blocking bit.
   */
  constexpr bool get_block() const {
    return _bits & (1 << 19);
  }

  /*
   * Derives a new Descriptor with the target key index replaced.
   */
  constexpr Descriptor with_target(unsigned t) const {
    return {(_bits & ~(0xF << 20)) | ((t & 0xF) << 20)};
  }

  /*
   * Extracts the target key index.
   */
  constexpr unsigned get_target() const {
    return (_bits >> 20) & 0xF;
  }

  /*
   * Derives a new Descriptor with the source key index replaced.
   */
  constexpr Descriptor with_source(unsigned s) const {
    return {(_bits & ~(0xF << 24)) | ((s & 0xF) << 24)};
  }

  /*
   * Extracts the source key index.
   */
  ETL_INLINE
  constexpr unsigned get_source() const {
    return (_bits >> 24) & 0xF;
  }

  /*
   * Derives a new Descriptor with the syscall number replaced.
   */
  constexpr Descriptor with_sysnum(unsigned s) const {
    return {(_bits & ~(0xF << 28)) | ((s & 0xF) << 28)};
  }

  /*
   * Extracts the syscall number.
   */
  constexpr unsigned get_sysnum() const {
    return (_bits >> 28) & 0xF;
  }

  /*
   * Derives a new Descriptor containing only the error and selector fields.
   * All other fields are zeroed.  This is intended to reduce information
   * leakage in messages delivered to application code.
   */
  ETL_INLINE
  constexpr Descriptor sanitized() const {
    return zero().with_selector(get_selector())
                 .with_error(get_error());
  }

  /*
   * Conversion to an integer.
   */
  constexpr explicit operator uint32_t() const {
    return _bits;
  }

  /*
   * Convenience predicate: does this descriptor describe an IPC call?
   */
  ETL_INLINE
  constexpr bool is_call() const {
    return get_receive_enabled() && get_source() == 0;
  }

  /*
   * Factory for Descriptors describing an IPC call on key 'k' with selector
   * 's'.
   */
  static constexpr Descriptor call(Selector s, unsigned k) {
    return Descriptor::zero()
      .with_sysnum(0)
      .with_selector(s)
      .with_send_enabled(true)
      .with_target(k)
      .with_receive_enabled(true)
      .with_source(0)
      .with_block(true);
  }

private:
  constexpr inline Descriptor(uint32_t bits) : _bits(bits) {}
  uint32_t _bits;
};

#endif  // COMMON_DESCRIPTOR_H
