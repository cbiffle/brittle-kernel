#include "a/sys/alloc.h"

#include "etl/array_count.h"

#include "a/sys/keys.h"
#include "a/sys/types.h"
#include "a/k/object_table.h"
#include "a/k/memory.h"
#include "a/rt/ipc.h"
#include "a/rt/keys.h"

#include "common/message.h"

#include "peanut_config.h"

namespace sys {

static constexpr auto allocation_failed = Exception(0x1c8af06d150e8638);


/*******************************************************************************
 * Slot allocator.  Serves up slots designated as "extra" in the AppInfo block.
 * Slots, once served, cannot be returned to the pool.
 */

// Number of slots we do not manage.
static constexpr unsigned fixed_slot_count =
  4 + config::memory_map_count + config::device_map_count;

// Number of slots we have handed out.
static unsigned slots_used = 0;

// Internal implementation of slot allocation.
static Maybe<TableIndex> alloc_slot_int() {
  if (slots_used == config::extra_slot_count) return nothing;
  return slots_used++ + fixed_slot_count;
}

// External version.
rt::AutoKey alloc_slot() {
  auto oti = alloc_slot_int().ref();
  return object_table::mint_key(ki::ot, oti, 0);
}


/*******************************************************************************
 * Memory allocator.
 */

// We maintain a linked list for every power-of-two block size.  For
// simplicity, we track all sizes, even the small ones we don't allow, and the
// big ones that we're unlikely to ever use.
//
// The number in mem_roots[x] is the Object Table index of the first free
// Memory object of size 2^(x+1).  That is, the mem_roots table is keyed by
// l2_half_size.
//
// The Object Table index of the *next* free object of size 2^(x+1) is stored
// in the first word of the Memory object.  Thus a linked list is formed.
//
// Zero is the Object Table index of Null, so we use it as an end-of-list
// marker (or, in mem_roots[x], an empty-list indicator).
static TableIndex mem_roots[31];

static rt::AutoKey mem_take(unsigned l2_half_size, uint64_t brand) {
  auto oti = mem_roots[l2_half_size];
  auto k = object_table::mint_key(ki::ot, oti, brand);
  mem_roots[l2_half_size] = memory::peek(k, 0);
  memory::poke(k, 0, 0);
  return k;
}

// Adds a Memory object to the freelist.  This is used during initialization,
// and could eventually be used to free Memory, once we allow that.
//
// Returns a flag indicating success; failure means the key was not Memory, or
// that its generation has advanced far enough to require intervention.
bool free_mem(KeyIndex key_in) {
  // Crack open the key to find its Object Table index.
  auto key_info = object_table::read_key(ki::ot, key_in);
  auto oti = key_info.index;

  // Defend against weird clients by checking the kind.
  if (object_table::get_kind(ki::ot, oti) != object_table::Kind::memory) {
    return false;
  }

  // Revoke outside access to this object.  Ours now.
  if (object_table::invalidate(ki::ot, oti) == false) return false;

  // Now, produce a fresh key.
  auto k_freed = object_table::mint_key(ki::ot, oti, 0);
  // Determine its properties.
  auto region = memory::inspect(k_freed);
  auto l2_half_size = region.get_l2_half_size();
  // Add it to the list.
  memory::poke(k_freed, 0, mem_roots[l2_half_size]);
  mem_roots[l2_half_size] = oti;

  // Deny access to that key we just made.
  rt::copy_key(k_freed, ki::null);

  return true;
}

// Allocates a Memory object of the requested size, if one is available.
// Splits larger objects if needed.
//
// Returns a dynamically allocated key register on success, or nothing on
// failure.
//
// Failure can occur if we either (1) run out of Slots to use when splitting,
// or (2) run out of actual Memory objects to split.
Maybe<rt::AutoKey> alloc_mem(unsigned target_l2_half_size, uint64_t brand) {
  // This algorithm is deliberately not recursive, because the recursive
  // version used an absurd amount of stack.

  static constexpr uint64_t internal_mem_brand = 0;  // TODO empower

  if (target_l2_half_size < 4) return nothing;  // Not satisfiable.

  // Massage the freelists so we have a memory block of the desired size, or
  // return false.
  {
    unsigned l2p;
    // Pass one: search up through the freelists to find a non-empty one.
    // Note that, if a block of the target size already exists, this leaves
    // l2p == target_l2_half_size.
    for (l2p = target_l2_half_size; l2p < etl::array_count(mem_roots); ++l2p) {
      if (mem_roots[l2p]) break;
    }

    // If we just ran off the top end of mem_roots, it means we're out of RAM.
    // (This is also where we catch large size parameters.)
    if (l2p >= etl::array_count(mem_roots)) return nothing;

    // Pass two: work back down through freelists, splitting at each step, until
    // we return to our target freelist.
    for (; l2p > target_l2_half_size; --l2p) {
      // Get a key to the head of this freelist and remove it.
      auto k_bot = mem_take(l2p, internal_mem_brand);
      auto k_bot_info = object_table::read_key(ki::ot, k_bot);

      // Allocate a slot for the new top-half object.
      auto maybe_top_oti = alloc_slot_int();
      if (!maybe_top_oti) return nothing;  // Out of slots!

      auto top_oti = maybe_top_oti.ref();

      // Mint a slot key.
      auto k_slot = object_table::mint_key(ki::ot, top_oti, 0);

      // Perform the split.  This technically has us using one more slot key
      // than we need to, but not for long.
      auto k_top = memory::split(k_bot, 1u << l2p, k_slot);

      // Return both halves to the freelist for the next-smaller size.
      memory::poke(k_top, 0, mem_roots[l2p - 1]);
      memory::poke(k_bot, 0, top_oti);
      mem_roots[l2p - 1] = k_bot_info.index;

      // Proceed to next freelist.
    }
  }

  // The code above has ensured that there is at least one memory object free
  // in mem_roots[target_l2_half_size], so now we just take it.
  return mem_take(target_l2_half_size, brand);
}

}  // namespace sys
