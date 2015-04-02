#include <gtest/gtest.h>

#include "k/address_range.h"
#include "k/object_table.h"

using etl::armv7m::Mpu;

namespace k {

ObjectTable object_table;

class AddressRangeTest_Small : public ::testing::Test {
protected:
  AddressRange _range {
    {reinterpret_cast<uint8_t *>(256), 256},
    false,
    AddressRange::ReadOnly::no,
  };

  Region const _reg {
    .rbar = Region::Rbar().with_addr_27(256 >> 5),
    .rasr = Region::Rasr().with_size(7),
  };

  Brand const _reg_brand{AddressRange::get_brand_for_region(_reg)};
};

TEST_F(AddressRangeTest_Small, begin_and_end) {
  ASSERT_EQ(reinterpret_cast<uint8_t *>(256),
            _range.get_region_begin(_reg_brand));
  ASSERT_EQ(reinterpret_cast<void *>(512), _range.get_region_end(_reg_brand));
}

TEST_F(AddressRangeTest_Small, make_key_legit) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_ap(Mpu::AccessPermissions::p_write_u_write),
  });

  auto maybe_key = _range.make_key(brand);
  ASSERT_TRUE(maybe_key);
}

TEST_F(AddressRangeTest_Small, make_key_fail_no_priv_access) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_ap(Mpu::AccessPermissions::p_none_u_none),
  });

  auto maybe_key = _range.make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must refuse to make keys that would deny privileged "
       "code access.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_reserved_bits_set) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    Region::Rasr(-1),
  });

  auto maybe_key = _range.make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys with reserved bits set, since we can't "
       "reason about what the hardware might do.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_explicit_region) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar.with_valid(true),
    _reg.rasr,
  });

  auto maybe_key = _range.make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys with explicit region numbers, as they "
       "would behave unpredictably when loaded.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_bad_size) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_size(1),
  });

  auto maybe_key = _range.make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys under the architectural minimum size, "
       "as the hardware response is undefined.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_too_big) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_size(uint8_t(_reg.rasr.get_size() + 2)),
  });

  auto maybe_key = _range.make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys that would extend outside the configured "
       "range.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_bad_base) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar.with_addr_27(1024),
    _reg.rasr,
  });

  auto maybe_key = _range.make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys that would extend outside the configured "
       "range.";
}

class AddressRangeTest_Stm32 : public ::testing::Test {
protected:
  AddressRange _range {
    {reinterpret_cast<uint8_t *>(0x08004000), 1032192},
    false,
    AddressRange::ReadOnly::no,
  };
};

TEST_F(AddressRangeTest_Stm32, actual_init_rom_works) {
  auto maybe_key = _range.make_key(0x300001108004000ULL);
  ASSERT_TRUE(maybe_key);
}

}  // namespace k

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
