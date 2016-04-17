#include <gtest/gtest.h>

#include "etl/array_count.h"

#include "k/address_range.h"
#include "k/object_table.h"
#include "k/null_object.h"
#include "k/region.h"

using etl::armv7m::Mpu;

namespace k {

class AddressRangeTest : public ::testing::Test {
protected:
  ObjectTable::Entry _entries[3];

  void SetUp() override {
    new(&_entries[0]) NullObject;

    set_object_table(new(&_entries[1]) ObjectTable);
    object_table().set_entries(_entries);

    new(&_entries[2]) AddressRange{get_actual_range()};

    for (unsigned i = 0; i < etl::array_count(_entries); ++i) {
      _entries[i].as_object().set_index(i);
    }
  }

  void TearDown() override {
    reset_object_table_for_test();
  }

  AddressRange & range() {
    return *static_cast<AddressRange *>(&_entries[2].as_object());
  }

private:
  virtual RangePtr<uint8_t> get_actual_range() = 0;
};

class AddressRangeTest_Small : public AddressRangeTest {
protected:
  RangePtr<uint8_t> get_actual_range() override {
    return {reinterpret_cast<uint8_t *>(256), 256};
  }

  Region const _reg {
    .rbar = Region::Rbar().with_addr_27(256 >> 5),
    .rasr = Region::Rasr().with_size(7),
  };

  Brand const _reg_brand{AddressRange::get_brand_for_region(_reg)};
};

TEST_F(AddressRangeTest_Small, begin_and_end) {
  ASSERT_EQ(reinterpret_cast<uint8_t *>(256),
            range().get_region_begin(_reg_brand));
  ASSERT_EQ(reinterpret_cast<void *>(512), range().get_region_end(_reg_brand));
}

TEST_F(AddressRangeTest_Small, make_key_legit) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_ap(Mpu::AccessPermissions::p_write_u_write),
  });

  auto maybe_key = range().make_key(brand);
  ASSERT_TRUE(maybe_key);
}

TEST_F(AddressRangeTest_Small, make_key_fail_no_priv_access) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_ap(Mpu::AccessPermissions::p_none_u_none),
  });

  auto maybe_key = range().make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must refuse to make keys that would deny privileged "
       "code access.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_reserved_bits_set) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    Region::Rasr(-1),
  });

  auto maybe_key = range().make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys with reserved bits set, since we can't "
       "reason about what the hardware might do.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_explicit_region) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar.with_valid(true),
    _reg.rasr,
  });

  auto maybe_key = range().make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys with explicit region numbers, as they "
       "would behave unpredictably when loaded.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_bad_size) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_size(1),
  });

  auto maybe_key = range().make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys under the architectural minimum size, "
       "as the hardware response is undefined.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_too_big) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar,
    _reg.rasr.with_size(uint8_t(_reg.rasr.get_size() + 2)),
  });

  auto maybe_key = range().make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys that would extend outside the configured "
       "range.";
}

TEST_F(AddressRangeTest_Small, make_key_fail_bad_base) {
  auto brand = AddressRange::get_brand_for_region({
    _reg.rbar.with_addr_27(1024),
    _reg.rasr,
  });

  auto maybe_key = range().make_key(brand);
  ASSERT_FALSE(maybe_key)
    << "AddressRange must deny keys that would extend outside the configured "
       "range.";
}

class AddressRangeTest_Stm32 : public AddressRangeTest {
protected:
  RangePtr<uint8_t> get_actual_range() override {
    return {reinterpret_cast<uint8_t *>(0x08004000), 1032192};
  }
};

TEST_F(AddressRangeTest_Stm32, actual_init_rom_works) {
  auto maybe_key = range().make_key(0x300001108004000ULL);
  ASSERT_TRUE(maybe_key);
}

}  // namespace k

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
