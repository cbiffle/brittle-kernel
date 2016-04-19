#include <gtest/gtest.h>

#include "etl/armv7m/mpu.h"

#include "k/memory.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/p2range.h"
#include "k/region.h"
#include "k/reply_sender.h"

#include "k/testutil/spy.h"

namespace k {

using Rbar = Region::Rbar;
using Rasr = Region::Rasr;
using etl::armv7m::Mpu;

class MemoryTest : public ::testing::Test {
protected:
  ObjectTable::Entry _entries[3];

  void SetUp() override {
    new (&_entries[0]) NullObject{0};

    {
      auto o = new(&_entries[1]) ObjectTable{0};
      set_object_table(o);
      o->set_entries(_entries);
    }

    new(&_entries[2]) Memory{0, p2range_under_test()};
  }

  void TearDown() override {
    reset_object_table_for_test();
  }

  Memory & memory() {
    return *static_cast<Memory *>(&_entries[2].as_object());
  }

  virtual P2Range p2range_under_test() = 0;
};

class MemoryTest_Small : public MemoryTest {
protected:
  P2Range p2range_under_test() override {
    return P2Range::of(256, 6).ref();
  }
};

TEST_F(MemoryTest_Small, change_disable_subregion) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_srd(0x00);
  auto brand = uint32_t(initial_rasr) >> 8;

  auto target_rasr = initial_rasr.with_srd(0x01);

  Spy spy{0};
  ReplySender sender{{Descriptor::call(1, 0), uint32_t(target_rasr)}};
  sender.set_key(0, spy.make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_TRUE(m.d0.get_error())
    << "disabling a subregion in an object < 256 bytes should be an error";
  ASSERT_EQ(uint64_t(Exception::bad_argument), (uint64_t(m.d2) << 32) | m.d1)
    << "exception should be bad_argument";
}

class MemoryTest_Typical : public MemoryTest {
protected:
  P2Range p2range_under_test() override {
    return P2Range::of(256, 7).ref();
  }
};

TEST_F(MemoryTest_Typical, inspect) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write);
  auto brand = uint32_t(initial_rasr) >> 8;

  Spy spy{0};
  ReplySender sender{{Descriptor::call(0, 0)}};
  sender.set_key(0, spy.make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_EQ(0, uint32_t(m.d0))
    << "inspecting a key should always succeed";

  auto rbar = Rbar(m.d1);

  ASSERT_EQ(p2range_under_test().base(),
            rbar.get_addr_27() << 5)
    << "Base address should be conveyed unchanged";
  ASSERT_FALSE(rbar.get_valid()) << "Region number valid field must not be set";
  ASSERT_EQ(0, rbar.get_region()) << "Region number field must not be set";

  auto rasr = Rasr(m.d2);
  auto expected_rasr = initial_rasr.with_size(7).with_enable(true);
  ASSERT_EQ(uint32_t(expected_rasr), uint32_t(rasr))
    << "Revealed RASR should be initial RASR but with size and enable set";
}

TEST_F(MemoryTest_Typical, change_tex) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_tex(0);
  auto brand = uint32_t(initial_rasr) >> 8;

  auto target_rasr = initial_rasr.with_tex(3);

  Spy spy{0};
  ReplySender sender{{Descriptor::call(1, 0), uint32_t(target_rasr)}};
  sender.set_key(0, spy.make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_EQ(0, uint32_t(m.d0))
    << "altering TEX should always succeed";

  auto & k = spy.keys().keys[1];
  ASSERT_EQ(memory().get_generation(), k.get_generation())
    << "returned key must have right generation";
  ASSERT_EQ(&memory(), k.get())
    << "returned key must point to object under test";
  ASSERT_EQ(uint32_t(target_rasr) >> 8, k.get_brand())
    << "returned key's brand must reflect new RASR";
}

TEST_F(MemoryTest_Typical, change_disable_subregion) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_srd(0x01);
  auto brand = uint32_t(initial_rasr) >> 8;

  auto target_rasr = initial_rasr.with_srd(0x81);

  Spy spy{0};
  ReplySender sender{{Descriptor::call(1, 0), uint32_t(target_rasr)}};
  sender.set_key(0, spy.make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_EQ(0, uint32_t(m.d0))
    << "disabling a new subregion in a typical sized object should succeed";

  auto & k = spy.keys().keys[1];
  ASSERT_EQ(memory().get_generation(), k.get_generation())
    << "returned key must have right generation";
  ASSERT_EQ(&memory(), k.get())
    << "returned key must point to object under test";
  ASSERT_EQ(uint32_t(target_rasr) >> 8, k.get_brand())
    << "returned key's brand must reflect new RASR";
}

TEST_F(MemoryTest_Typical, change_enable_subregion) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_srd(0x81);
  auto brand = uint32_t(initial_rasr) >> 8;

  auto target_rasr = initial_rasr.with_srd(0x01);

  Spy spy{0};
  ReplySender sender{{Descriptor::call(1, 0), uint32_t(target_rasr)}};
  sender.set_key(0, spy.make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_TRUE(m.d0.get_error())
    << "enabling a disabled subregion should fail";
  ASSERT_EQ(uint64_t(Exception::bad_argument), (uint64_t(m.d2) << 32) | m.d1)
    << "exception should be bad_argument";
}

}  // namespace k

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
