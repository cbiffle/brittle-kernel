#include "k/memory.h"

#include "etl/armv7m/mpu.h"

#include "common/abi_sizes.h"
#include "common/message.h"
#include "common/descriptor.h"

#include "k/sender.h"
#include "k/slot.h"
#include "k/region.h"
#include "k/reply_sender.h"

using etl::armv7m::Mpu;

namespace k {

template struct ObjectSubclassChecks<Memory, kabi::memory_size>;

Memory::Memory(Generation g, P2Range range)
  : Object{g}, _range{range} {}

Region Memory::get_region_for_brand(Brand brand) const {
  return {
    Region::Rbar()
      .with_addr_27(_range.base() >> 5),
    Region::Rasr(uint32_t(brand) << 8)
      .with_size(uint8_t(_range.l2_half_size()))
      .with_enable(true),
  };
}

bool Memory::is_memory() const {
  return true;
}

void Memory::deliver_from(Brand const & brand, Sender * sender) {
  Keys k;
  Message m = sender->on_delivery_accepted(k);

  switch (m.d0.get_selector()) {
    case 0: 
      do_inspect(brand, m, k);
      break;

    case 1:
      do_change(brand, m, k);
      break;

    case 2:
      do_split(brand, m, k);
      break;

    default:
      do_badop(m, k);
      break;
  }
}

void Memory::do_inspect(Brand const & brand,
                        Message const &,
                        Keys & k) {
  auto reg = get_region_for_brand(brand);

  ScopedReplySender reply_sender{k.keys[0], {
    Descriptor::zero(),
    uint32_t(reg.rbar),
    uint32_t(reg.rasr),
  }};
}

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
      ETL_ASSERT(false);
  }
#undef CASE
}

static bool ap_is_stronger(Mpu::AccessPermissions a, Mpu::AccessPermissions b) {
  auto da = decode_ap(a);
  auto db = decode_ap(b);

  return da.priv > db.priv || da.unpriv > db.unpriv;
}

void Memory::do_change(Brand const & brand,
                       Message const & m,
                       Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

  auto rasr_dirty = Region::Rasr(m.d1);

  // Copy the defined and relevant fields into a new value.  This is equivalent
  // to a very wordy bitmask.
  auto rasr = Region::Rasr()
    .with_xn(rasr_dirty.get_xn())
    .with_ap(rasr_dirty.get_ap())
    .with_tex(rasr_dirty.get_tex())
    .with_s(rasr_dirty.get_s())
    .with_c(rasr_dirty.get_c())
    .with_b(rasr_dirty.get_b())
    .with_srd(rasr_dirty.get_srd());

  auto current = get_region_for_brand(brand).rasr;

  if (ap_is_unpredictable(rasr.get_ap())
      || ap_is_stronger(rasr.get_ap(), current.get_ap())
      || (current.get_srd() & ~rasr.get_srd())
      || (_range.l2_size() < 8 && rasr.get_srd())) {
    reply_sender.get_message() = Message::failure(Exception::bad_argument);
    return;
  }

  auto new_brand = uint32_t(rasr) >> 8;
  reply_sender.set_key(1, make_key(new_brand).ref());
}

void Memory::do_split(Brand const & brand,
                      Message const & m,
                      Keys & k) {
  ScopedReplySender reply_sender{k.keys[0]};

  auto & donation_key = k.keys[1];

  if (get_region_for_brand(brand).rasr.get_srd()) {
    // Can't split, some subregions are disabled.
    reply_sender.get_message() = Message::failure(Exception::bad_operation);
    return;
  }

  auto maybe_bottom = _range.bottom();
  auto maybe_top = _range.top();

  if (!maybe_bottom || !maybe_top) {
    // Can't split, too small.
    reply_sender.get_message() = Message::failure(Exception::bad_operation);
    return;
  }

  auto & bottom = maybe_bottom.ref();
  auto & top = maybe_top.ref();

  auto objptr = donation_key.get();
  if (!objptr->is_slot()) {
    // Can't split, donation was of wrong type.
    reply_sender.get_message() = Message::failure(Exception::bad_kind);
    return;
  }

  // Note that, since the object pointer is_slot, it does not alias this.

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
  static_cast<Slot *>(objptr)->~Slot();
  // Create the Memory object, implicitly revoking keys to the Slot.
  auto memptr = new(objptr) Memory{other_generation + 1, top};
  // Provide a key.
  reply_sender.set_key(2, memptr->make_key(brand).ref());
}

}  // namespace k
