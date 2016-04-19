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

}  // namespace k
