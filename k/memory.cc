#include "k/memory.h"

#include "etl/armv7m/mpu.h"

#include "common/abi_sizes.h"
#include "common/message.h"
#include "common/descriptor.h"

#include "k/sender.h"
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
      || ap_is_stronger(rasr.get_ap(), current.get_ap())) {
    reply_sender.get_message() = Message::failure(Exception::bad_argument);
    return;
  }

  auto new_brand = uint32_t(rasr) >> 8;
  reply_sender.set_key(1, make_key(new_brand).ref());
}

}  // namespace k
