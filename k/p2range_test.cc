#include "k/p2range.h"

/*
 * Static tests for the P2Range type.  There's no need to actually run the
 * binary this produces.
 */

using namespace k;

/*
 * Basic properties of 'all'
 */

static constexpr P2Range all = P2Range::all();

static_assert(all.base() == 0,
    "all() must start at address zero.");
static_assert(all.l2_size() == 32,
    "all()'s size must be 32 (log 2).");
static_assert(all.l2_half_size() == 31,
    "all()'s half size must be 31 (log 2).");
static_assert(all.half_size() == 1u << 31,
    "all()'s half size must be 2^31.");
static_assert(all.is_smallest() == false,
    "all() must not be the smallest range.");

/*
 * Splitting 'all'
 */
static_assert(all.bottom(),
    "all() must have a bottom half");
static_assert(all.top(),
    "all() must have a top half");

static constexpr P2Range all_bottom = all.bottom().const_ref();
static constexpr P2Range all_top = all.top().const_ref();

static_assert(
    all_bottom.base() == all.base(),
    "all()'s bottom must have the same base");
static_assert(
    all_bottom.l2_size() == all.l2_half_size(),
    "all()'s bottom must be half the size (log 2)");
static_assert(
    all_bottom.half_size() == all.half_size() / 2,
    "all()'s bottom must be half the size");

static_assert(
    all_top.base() == all.base() + all.half_size(),
    "all()'s top's base must be displaced by the half-size.");
static_assert(
    all_top.l2_size() == all.l2_half_size(),
    "all()'s top must be half the size (log 2)");
static_assert(
    all_top.half_size() == all.half_size() / 2,
    "all()'s top must be half the size");

/*
 * Making arbitrary ranges
 */

static_assert(P2Range::of(0, 31), "of(0, 31) is defined");
static constexpr P2Range r_0_31 = P2Range::of(0, 31).const_ref();
static_assert(
    r_0_31.base() == 0,
    "of(0, 31)'s base is 0");
static_assert(
    r_0_31.l2_half_size() == 31,
    "of(0, 31)'s l2_half_size is 31");

static_assert(!P2Range::of(0, 32), "of(0, 32) is not defined");

static_assert(P2Range::of(0, 4), "of(0, 4) is defined");
static constexpr P2Range r_0_4 = P2Range::of(0, 4).const_ref();
static_assert(
    r_0_4.base() == 0,
    "of(0, 4)'s base is 0");
static_assert(
    r_0_4.l2_half_size() == 4,
    "of(0, 4)'s l2_half_size is 4");

static_assert(!P2Range::of(0, 3), "of(0, 3) is not defined");

/*
 * Small ranges.
 */
static constexpr P2Range r_0_5 = P2Range::of(0, 5).const_ref();

static_assert(r_0_5.is_smallest() == false,
    "{0, 5} is not the smallest");
static_assert(r_0_5.bottom(),
    "{0, 5} has a bottom");
static_assert(r_0_5.top(),
    "{0, 5} has a top");

static constexpr P2Range
  r_0_5_b = r_0_5.bottom().const_ref(),
  r_0_5_t = r_0_5.top().const_ref();

static_assert(r_0_5_b.is_smallest() == true,
    "{0, 5}.bottom() is now smallest");
static_assert(r_0_5_t.is_smallest() == true,
    "{0, 5}.top() is now smallest");

static_assert(r_0_5_b.l2_half_size() == 4, "");
static_assert(r_0_5_t.l2_half_size() == 4, "");

static_assert(r_0_5_b.base() == r_0_5.base(),
    "{0, 5}.bottom() leaves base unchanged");

static_assert(r_0_5_t.base() == r_0_5.base() + 32,
    "{0, 5}.top() shifts base by 32");

/*
 * Containment.
 */

static_assert(P2Range::all().lsb_mask() == 0xFFFFFFFF, "");
static_assert(P2Range::of(0, 4).const_ref().lsb_mask() == 0x1F, "");

static_assert(P2Range::all().contains(P2Range::of(0, 5).const_ref()), "");
static_assert(P2Range::all().contains(P2Range::of(0, 30).const_ref()), "");
static_assert(P2Range::all().contains(P2Range::of(0, 31).const_ref()), "");

static_assert(P2Range::of(256, 7).const_ref().contains(P2Range::of(256, 7).const_ref()), "");
static_assert(P2Range::of(256, 7).const_ref().contains(P2Range::of(256, 6).const_ref()), "");
static_assert(P2Range::of(256, 7).const_ref().contains(P2Range::of(384, 6).const_ref()), "");
static_assert(!P2Range::of(256, 7).const_ref().contains(P2Range::of(512, 6).const_ref()), "");

int main() {
  return 0;
}
