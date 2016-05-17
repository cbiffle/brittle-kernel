#include "k/memory.h"

#include "etl/destroy.h"
#include "etl/armv7m/mpu.h"

#include "common/abi_sizes.h"
#include "common/message.h"
#include "common/descriptor.h"

#include "k/become.h"
#include "k/context.h"
#include "k/panic.h"
#include "k/region.h"
#include "k/reply_sender.h"
#include "k/scheduler.h"
#include "k/sender.h"
#include "k/slot.h"

using etl::armv7m::Mpu;

namespace k {

template struct ObjectSubclassChecks<Memory, kabi::memory_size>;

/*
 * Utility function for checking that one integer range contains another.
 */
static constexpr bool contains(uintptr_t parent_base, size_t parent_size,
                               uintptr_t child_base, size_t child_size) {
  return child_base >= parent_base
    && (child_base - parent_base) <= parent_size
    && (child_base + child_size - parent_base) <= parent_size;
}


/*******************************************************************************
 * Construction, destruction, and basic properties.
 */

Memory::Memory(Generation g,
               uintptr_t base,
               size_t size,
               uint32_t attributes,
               Memory * parent)
  : Object{g},
    _base{base},
    _size_bytes{size},
    _attributes{attributes},
    _parent{parent},
    _child_count{0}
{
  PANIC_IF(base + size < base, "mem base+size overflow");

  // Detect mappable regions at creation time and cache details.  Mappable
  // regions are at least 32 bytes in size, are a power of two in size, and are
  // naturally aligned with respect to their size.
  if (_size_bytes >= 32
      && (_size_bytes & (_size_bytes - 1)) == 0
      && (_base & (_size_bytes - 1)) == 0) {
    // Store this decision and cache the log2 of the size.
    unsigned l2_half_size;
    for (l2_half_size = 4; l2_half_size < 31; ++l2_half_size) {
      if (_size_bytes == (2u << l2_half_size)) break;
    }
    _attributes = (_attributes & ~cached_l2hs_mask)
                | (l2_half_size << cached_l2hs_lsb)
                | mappable_attribute_mask;
  } else {
    _attributes = _attributes & ~(cached_l2hs_mask | mappable_attribute_mask);
  }

  if (parent) {
    // The caller is expected to have checked the extent of this object vs. the
    // parent already.
    PANIC_UNLESS(contains(parent->_base, parent->_size_bytes,
                          _base, _size_bytes),
             "bad child geometry");
    ++parent->_child_count;
  }
}

Memory::~Memory() {
  PANIC_IF(_child_count, "non-leaf Memory dtor");

  if (parent()) {
    --_parent->_child_count;
    _parent = nullptr;
  }
}

Region Memory::get_region_for_brand(Brand const & brand) const {
  if (!is_mappable()) {
    // If an object is not mappable, it confers no authority when loaded into
    // an MPU region register.
    return { {}, {} };
  }

  return {
    Region::Rbar()
      .with_addr_27(_base >> 5),
    Region::Rasr(uint32_t(brand) << 8)
      .with_size(uint8_t(get_cached_l2_half_size()))
      .with_enable(true),
  };
}

void Memory::invalidation_hook() {
  // Invalidation leaves the hierarchy intact.  It is legal to invalidate an
  // object with children.

  // We can't currently tell whether *this* Memory object is relevant for the
  // MPU configuration, but invalidating the whole thing is pretty cheap.
  current->apply_to_mpu();
}


/*******************************************************************************
 * Utility functions for reasoning about MPU permissions.
 */

static bool ap_is_unpredictable(Mpu::AccessPermissions ap) {
  // Per table B3-15 in ARMv7-M ARM
  return uint32_t(ap) > 0b111 || uint32_t(ap) == 0b100;
}

enum class Access { none, read, write };
struct SplitAccess { Access priv, unpriv; };

static SplitAccess decode_ap(Mpu::AccessPermissions ap) {
#define CASE(p, u) \
  case Mpu::AccessPermissions::p_##p##_u_##u: \
    return { Access::p, Access::u };

  switch (ap) {
    CASE(none, none)
    CASE(read, none)
    CASE(read, read)
    CASE(write, none)
    CASE(write, read)
    CASE(write, write)

    default:
      // This case should have been prevented by a call to ap_is_unpredictable,
      // above.
      PANIC("decoding unpredictable AP");
  }
#undef CASE
}

static bool ap_is_stronger(Mpu::AccessPermissions a, Mpu::AccessPermissions b) {
  auto da = decode_ap(a);
  auto db = decode_ap(b);

  return da.priv > db.priv || da.unpriv > db.unpriv;
}

/*
 * Lifts relevant and defined fields from a user-provided RASR value, leaving
 * undefined and irrelevant bits behind.
 */
static constexpr Region::Rasr scrub_rasr(Region::Rasr dirty) {
  return Region::Rasr()
    .with_xn(dirty.get_xn())
    .with_ap(dirty.get_ap())
    .with_tex(dirty.get_tex())
    .with_s(dirty.get_s())
    .with_c(dirty.get_c())
    .with_b(dirty.get_b())
    .with_srd(dirty.get_srd());
}


/*******************************************************************************
 * Implementation of the Memory protocol.
 */

void Memory::deliver_from(Brand const & brand, Sender * sender) {
  Keys k;
  Message m = sender->on_delivery(k);

  ScopedReplySender reply_sender{k.keys[0]};

  switch (m.desc.get_selector()) {
    case 0:  // inspect
      {
        auto reg = get_region_for_brand(brand);
        reply_sender.message().d0 = _base;
        reply_sender.message().d1 = uint32_t(reg.rasr);
        reply_sender.message().d2 = _size_bytes;
        reply_sender.message().d3 = _attributes & abi_attributes_mask;
      }
      return;

    case 1:  // change
      {
        // TODO: this should be redefined to make sense for any Memory object.
        if (!is_mappable()) {
          reply_sender.message() = Message::failure(Exception::bad_operation);
          return;
        }

        auto new_rasr = scrub_rasr(Region::Rasr(m.d0));
        auto current_rasr = get_region_for_brand(brand).rasr;

        if (ap_is_unpredictable(new_rasr.get_ap())
            || ap_is_stronger(new_rasr.get_ap(), current_rasr.get_ap())
            || (current_rasr.get_srd() & ~new_rasr.get_srd())
            || (_size_bytes < 256 && new_rasr.get_srd())) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        auto new_brand = uint32_t(new_rasr) >> 8;
        reply_sender.set_key(1, make_key(new_brand).ref());
      }
      return;

    case 2:  // split
      do_split(reply_sender, brand, m, k);
      return;

    case 3:  // become
      if (get_region_for_brand(brand).rasr.get_srd()) {
        // Can't transmogrify, some subregions are disabled.
        reply_sender.message() = Message::failure(Exception::bad_operation);
        return;
      }

      become(*this, m, k, reply_sender.rs);
      return;

    case 4:  // peek
    case 5:  // poke
      {
        auto offset = m.d0;
        auto size_in_words = _size_bytes / sizeof(uint32_t);

        if (offset >= size_in_words) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        // TODO: should use the caching/ordering specified by the key.  We can
        // do this from kernel code by temporarily replacing one of the MPU
        // regions with the one in the key.  Should be region #7 so it takes
        // priority over potential aliases elsewhere in the MPU.

        // Note: since the address is contained within the body of this Memory
        // object, we do not need to use ldrt/strt to access it.  This is
        // important!  ARMv7-M defines areas of address space, particularly
        // the SysTick Timer, that are *inaccessible* to unprivileged code,
        // even with MPU adjustments.  This fixes that.
        auto ptr = reinterpret_cast<uint32_t *>(_base) + offset;
        if (m.desc.get_selector() == 4) {  // peek
          reply_sender.message().d0 = *ptr;
        } else {  // poke
          *ptr = m.d1;
        }
      }
      return;

    case 6:  // make child
      {
        if (get_region_for_brand(brand).rasr.get_srd()) {
          reply_sender.message() = Message::failure(Exception::bad_operation);
          return;
        }

        auto child_base = m.d0, child_size = m.d1;

        if (!contains(_base, _size_bytes, child_base, child_size)) {
          reply_sender.message() = Message::failure(Exception::bad_argument);
          return;
        }

        auto objptr = k.keys[1].get();
        if (objptr->get_kind() != Kind::slot) {
          reply_sender.message() = Message::failure(Exception::bad_kind);
          return;
        }

        auto slot_generation = objptr->get_generation();
        etl::destroy(*static_cast<Slot *>(objptr));
        auto child = new(objptr) Memory{slot_generation + 1,
                                        child_base,
                                        child_size,
                                        _attributes,
                                        this};
        reply_sender.set_key(1, child->make_key(brand).ref());
      }
      return;

    default:
      reply_sender.message() =
        Message::failure(Exception::bad_operation, m.desc.get_selector());
      return;
  }
}

void Memory::do_split(ScopedReplySender & reply_sender,
                      Brand const & brand,
                      Message const & m,
                      Keys & k) {
  auto split_pos = m.d0;

  if (split_pos > _size_bytes) {
    reply_sender.message() = Message::failure(Exception::bad_argument);
    return;
  }

  // We can't split if: subregions are disabled, or there are children.
  if (get_region_for_brand(brand).rasr.get_srd()
      || child_count()) {
    reply_sender.message() = Message::failure(Exception::bad_operation);
    return;
  }

  auto & donation_key = k.keys[1];
  auto slot = donation_key.get();
  if (slot->get_kind() != Kind::slot) {
    // Can't split, donation was of wrong type.
    reply_sender.message() = Message::failure(Exception::bad_kind);
    return;
  }
  // Note that, since slot indicates its Kind is slot, it does not alias
  // this.

  // Commit point

  // Rewrite the donated slot object.
  {
    // Record the generation so we can +1 it.
    auto generation = slot->get_generation();
    // Ensure that Slot's destructor is run (currently meaningless, but good
    // practice).
    etl::destroy(*static_cast<Slot *>(slot));
    // Create the Memory object, implicitly revoking keys to the Slot.
    auto top = new(slot) Memory{
        generation + 1,
        _base + split_pos,
        _size_bytes - split_pos,
        _attributes};
    // Provide a key.
    reply_sender.set_key(2, top->make_key(brand).ref());
  }

  // Rewrite this object to become the lower piece.
  {
    // Back up details.
    auto generation = get_generation();
    auto base = _base;
    auto atts = _attributes;
    // Destroy us.
    etl::destroy(*this);
    // Resurrect.
    auto bot = new(this) Memory{generation + 1, base, split_pos, atts};
    // Provide a key to the resulting shrunk version.
    reply_sender.set_key(1, bot->make_key(brand).ref());
  }

  // Update MPU, in case the split object was in the current Context's memory
  // map.
  current->apply_to_mpu();
}

}  // namespace k
