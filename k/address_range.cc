#include "k/address_range.h"

#include "etl/armv7m/mpu.h"

#include "common/message.h"
#include "common/descriptor.h"

#include "k/sender.h"
#include "k/region.h"
#include "k/reply_sender.h"

using etl::armv7m::Mpu;

namespace k {

AddressRange::AddressRange(RangePtr<uint8_t> range,
                           bool prevent_execution,
                           ReadOnly read_only)
  : _range{range},
    _xn{prevent_execution},
    _read_only{read_only} {}

Region AddressRange::get_region_for_brand(Brand brand) const {
  return { Region::Rbar(uint32_t(brand)), Region::Rasr(uint32_t(brand >> 32)) };
}

Brand AddressRange::get_brand_for_region(Region region) {
  return Brand(uint32_t(region.rbar)) | (Brand(uint32_t(region.rasr)) << 32);
}

void * AddressRange::get_region_begin(Brand brand) const {
  auto region = get_region_for_brand(brand);
  return reinterpret_cast<void *>(region.rbar.get_addr_27() << 5);
}

void * AddressRange::get_region_end(Brand brand) const {
  auto region = get_region_for_brand(brand);
  auto begin = region.rbar.get_addr_27() << 5;
  auto size = uintptr_t(1) << (region.rasr.get_size() + 1);
  return reinterpret_cast<void *>(begin + size);
}

static bool ap_valid(Mpu::AccessPermissions ap) {
  switch (ap) {
    case Mpu::AccessPermissions::p_none_u_none:
    case Mpu::AccessPermissions::p_write_u_none:
    case Mpu::AccessPermissions::p_write_u_read:
    case Mpu::AccessPermissions::p_write_u_write:
    case Mpu::AccessPermissions::p_read_u_none:
    case Mpu::AccessPermissions::p_read_u_read:
      return true;

    default:
      return false;
  }
}

static bool priv_reads_allowed(Mpu::AccessPermissions ap) {
  return ap != Mpu::AccessPermissions::p_none_u_none;
}

static bool priv_writes_allowed(Mpu::AccessPermissions ap) {
  return (unsigned(ap) & 0b100) != 0b100;
}

static bool unpriv_writes_allowed(Mpu::AccessPermissions ap) {
  return ap == Mpu::AccessPermissions::p_write_u_write;
}

Maybe<Key> AddressRange::make_key(Brand brand) {
  auto region = get_region_for_brand(brand);

  // Any ones in reserved bit positions are suspicious.  Since the ETL API
  // gives us no way to access reserved bits, we'll have to do something
  // slightly ugly.
  if (uint32_t(region.rasr) & 0xE8C000C0) return nothing;

  // It's not cool for a key to imply a region number; keys must be usable in
  // any region slot.
  if (region.rbar.get_valid() || region.rbar.get_region() != 0) return nothing;

  // Check for sizes outside of valid range.
  if (region.rasr.get_size() < 4) return nothing;

  // Ensure that the memory region described falls entirely within our purview.
  auto begin = reinterpret_cast<uint8_t *>(region.rbar.get_addr_27() << 5);
  auto size  = (1 << (region.rasr.get_size() + 1));
  auto last  = begin + size - 1;  // inclusive

  if (!_range.contains(begin)) return nothing;
  if (!_range.contains(last)) return nothing;

  // Check the read/write permissions.
  // We require privileged reads to be permanently enabled.
  // The rest is negotiable.
  auto ap = region.rasr.get_ap();

  if (!ap_valid(ap)) return nothing;

  if (!priv_reads_allowed(ap)) return nothing;  // must always be permitted

  switch (_read_only) {
    case ReadOnly::priv:
      if (priv_writes_allowed(ap)) return nothing;
      // fall through.

    case ReadOnly::unpriv:
      if (unpriv_writes_allowed(ap)) return nothing;
      // fall through.
    
    case ReadOnly::no:
      break;
  }

  // Check execute permission.
  if (_xn && !region.rasr.get_xn()) return nothing;

  // TODO: should we allow the AddressRange to enforce anything about memory
  // ordering or cache policy?

  // Checks pass; defer to superclass implementation.
  return Object::make_key(brand);
}

bool AddressRange::is_address_range() const {
  return true;
}

void AddressRange::deliver_from(Brand const & brand, Sender * sender) {
  Message m;
  Keys k;
  sender->on_delivery_accepted(m, k);

  switch (m.d0.get_selector()) {
    case 0: 
      do_inspect(brand, m, k);
      break;

    default:
      do_badop(m, k);
      break;
  }
}

void AddressRange::do_inspect(Brand const & brand,
                              Message const & m,
                              Keys & k) {
  ReplySender reply_sender{{
    Descriptor::zero(),
    uint32_t(brand),
    uint32_t(brand >> 32),
  }};
  k.keys[0].deliver_from(&reply_sender);
}

}  // namespace k
