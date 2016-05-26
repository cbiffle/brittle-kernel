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

Memory::Memory(Generation g, P2Range range, uint32_t attributes)
  : Object{g},
    _range{range},
    _attributes{attributes}
    {}

Region Memory::get_region_for_brand(Brand const & brand) const {
  return {
    Region::Rbar()
      .with_addr_27(_range.base() >> 5),
    Region::Rasr(uint32_t(brand) << 8)
      .with_size(uint8_t(_range.l2_half_size()))
      .with_enable(true),
  };
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
        reply_sender.message().d0 = uint32_t(reg.rbar);
        reply_sender.message().d1 = uint32_t(reg.rasr);
      }
      return;

    case 1:  // change
      {
        auto new_rasr = scrub_rasr(Region::Rasr(m.d0));
        auto current_rasr = get_region_for_brand(brand).rasr;

        if (ap_is_unpredictable(new_rasr.get_ap())
            || ap_is_stronger(new_rasr.get_ap(), current_rasr.get_ap())
            || (current_rasr.get_srd() & ~new_rasr.get_srd())
            || (_range.l2_size() < 8 && new_rasr.get_srd())) {
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
        auto size_in_words = _range.half_size() / 2;

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
        auto ptr = reinterpret_cast<uint32_t *>(_range.base()) + offset;
        if (m.desc.get_selector() == 4) {  // peek
          reply_sender.message().d0 = *ptr;
        } else {  // poke
          *ptr = m.d1;
        }
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
  auto & donation_key = k.keys[1];

  if (get_region_for_brand(brand).rasr.get_srd()) {
    // Can't split, some subregions are disabled.
    reply_sender.message() = Message::failure(Exception::bad_operation);
    return;
  }

  auto maybe_bottom = _range.bottom();
  auto maybe_top = _range.top();

  if (!maybe_bottom || !maybe_top) {
    // Can't split, too small.
    reply_sender.message() = Message::failure(Exception::bad_operation);
    return;
  }

  auto & bottom = maybe_bottom.ref();
  auto & top = maybe_top.ref();

  auto objptr = donation_key.get();
  if (objptr->get_kind() != Kind::slot) {
    // Can't split, donation was of wrong type.
    reply_sender.message() = Message::failure(Exception::bad_kind);
    return;
  }
  // Note that, since objptr indicates its Kind is slot, it does not alias
  // this.

  // Commit point

  // Invalidate all existing keys to this object.
  set_generation(get_generation() + 1);
  // Shrink this object to the bottom of the range.
  _range = bottom;
  // Provide a key to the resulting shrunk version.
  reply_sender.set_key(1, make_key(brand).ref());

  // Rewrite the donated slot object.
  // Record the generation so we can +1 it.
  auto other_generation = objptr->get_generation();
  // Ensure that Slot's destructor is run (currently meaningless, but good
  // practice).
  etl::destroy(*static_cast<Slot *>(objptr));
  // Create the Memory object, implicitly revoking keys to the Slot.
  auto memptr = new(objptr) Memory{other_generation + 1, top, _attributes};
  // Provide a key.
  reply_sender.set_key(2, memptr->make_key(brand).ref());

  // Update MPU, in case the split object was in the current Context's memory
  // map.
  current->apply_to_mpu();
}

}  // namespace k
