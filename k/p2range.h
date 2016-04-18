#ifndef K_P2RANGE_H
#define K_P2RANGE_H

/*
 * A P2Range is a literal, pointer-like type that designates a
 * naturally-aligned power-of-two-sized range of the 32-bit non-negative
 * integers (usually interpreted as addresses).  Ranges must have size of at
 * least 5 due to the encoding used; because of the role P2Ranges play in the
 * kernel -- namely, describing MPU regions, which have the same 32-byte
 * minimum -- this isn't a hardship.
 *
 * Note that while P2Ranges are similar to the region encoding used by the
 * ARMv7-M MPU, the actual encoding is different, because of our need to
 * indicate base and size in one word, and attributes in another.
 */

#include <cstdint>

#include "k/maybe.h"

namespace k {

class P2Range {
public:
  /*
   * The range that includes everything.
   */
  static constexpr P2Range all() {
    return P2Range{0, 31};
  }

  /*
   * Factory function for arbitrary ranges, with checking.
   */
  static constexpr Maybe<P2Range> of(uint32_t new_base,
                                     unsigned new_l2_half_size) {
    return
      (new_l2_half_size < min_l2_half_size
       || new_l2_half_size > 31
       || (new_base & ((2u << new_l2_half_size) - 1)) != 0)
        ? nothing
        : Maybe<P2Range>{{new_base, new_l2_half_size}};
  }

  /*
   * Extracts the base of this range, encoded as an integer.
   */
  constexpr uint32_t base() const {
    return _encoding & ~l2_size_mask;
  }

  /*
   * Extracts the base-2 logarithm of *half* of this range's size (i.e. the
   * log2-size minus one).  This is here because we can produce it for free,
   * but l2_size() (below) is often more useful.
   */
  constexpr unsigned l2_half_size() const {
    return _encoding & l2_size_mask;
  }

  /*
   * Extracts the base-2 logarithm of this size of this range.
   */
  constexpr unsigned l2_size() const { return l2_half_size() + 1; }

  /*
   * Generates the size of *half* of this range.  This number is guaranteed to
   * fit into a word, unlike the full size.
   */
  constexpr unsigned half_size() const { return 1u << l2_half_size(); }

  /*
   * Checks whether this range's size is the smallest size that can be
   * described.
   */
  constexpr bool is_smallest() const {
    return l2_half_size() == min_l2_half_size;
  }

  /*
   * Derives the P2Range describing the bottom half of this one, if this range
   * is large enough to be split.
   *
   * The bottom half of a splittable range has the same base(), and the
   * l2_half_size() is reduced by one.
   */
  constexpr Maybe<P2Range> bottom() const {
    return
      is_smallest()
        ? nothing
        : Maybe<P2Range>{{base(), l2_half_size() - 1}};
  }

  /*
   * Derives the P2Range describing the top half of this one, if this range is
   * large enough to be split.
   *
   * The top half of a splittable range has base() advanced by 2^l2_half_size()
   * and the l2_half_size() is reduced by one.
   */
  constexpr Maybe<P2Range> top() const {
    return
      is_smallest()
        ? nothing
        : Maybe<P2Range>{{
            base() + half_size(),
            l2_half_size() - 1,
          }};
  }

  /*
   * Extracts the bitwise encoding of this range.
   */
  constexpr operator uint32_t() const {
    return _encoding;
  }

private:
  static constexpr uint32_t l2_size_mask = 0x1F;
  static constexpr uint32_t min_l2_half_size = 4;

  constexpr P2Range(uint32_t new_base, unsigned new_l2_size_minus_one)
    : _encoding{
      (new_base & ~l2_size_mask) | (new_l2_size_minus_one & l2_size_mask)}
  {}

  uint32_t _encoding;
};

}  // namespace k

#endif  // K_P2RANGE_H
