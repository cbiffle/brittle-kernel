#include <gtest/gtest.h>

#include "etl/armv7m/mpu.h"

#include "k/memory.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/p2range.h"
#include "k/region.h"
#include "k/reply_sender.h"
#include "k/slot.h"

#include "k/testutil/spy.h"

namespace k {

using Rbar = Region::Rbar;
using Rasr = Region::Rasr;
using etl::armv7m::Mpu;

class MemoryTest : public ::testing::Test {
protected:
  ObjectTable::Entry _entries[4];

  void SetUp() override {
    new (&_entries[0]) NullObject{0};

    {
      auto o = new(&_entries[1]) ObjectTable{0};
      set_object_table(o);
      o->set_entries(_entries);
    }

    new(&_entries[2]) Memory{0, p2range_under_test()};

    new(&_entries[3]) Slot{0};
  }

  void TearDown() override {
    reset_object_table_for_test();
  }

  Memory & memory() {
    return *static_cast<Memory *>(&_entries[2].as_object());
  }

  virtual P2Range p2range_under_test() = 0;
};


/*******************************************************************************
 * Tests at an arbitrarily-chosen "typical" size of 256 bytes.  This is large
 * enough that all operations are available, but small enough that we can still
 * merge.
 */

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

TEST_F(MemoryTest_Typical, split_without_donation) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write);
  auto brand = uint32_t(initial_rasr) >> 8;

  Spy spy{0};
  ReplySender sender{{Descriptor::call(2, 0)}};
  sender.set_key(0, spy.make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_TRUE(m.d0.get_error())
    << "splitting without a slot key should fail";
  ASSERT_EQ(uint64_t(Exception::bad_kind), (uint64_t(m.d2) << 32) | m.d1)
    << "exception should be bad_kind";
}

TEST_F(MemoryTest_Typical, split_srd_fail) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write)
      .with_srd(0x01);
  auto brand = uint32_t(initial_rasr) >> 8;

  Spy spy{0};
  ReplySender sender{{Descriptor::call(2, 0)}};
  sender.set_key(0, spy.make_key(0).ref());
  sender.set_key(1, _entries[3].as_object().make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_TRUE(m.d0.get_error())
    << "splitting a region with SRD set should fail";
  ASSERT_EQ(uint64_t(Exception::bad_operation), (uint64_t(m.d2) << 32) | m.d1)
    << "exception should be bad_operation";
}

TEST_F(MemoryTest_Typical, split_ok) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write);
  auto brand = uint32_t(initial_rasr) >> 8;

  Spy spy{0};
  ReplySender sender{{Descriptor::call(2, 0)}};
  sender.set_key(0, spy.make_key(0).ref());
  sender.set_key(1, _entries[3].as_object().make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_EQ(0, uint32_t(m.d0))
    << "splitting a typical-size object with a valid slot donation"
       " should succeed";

  /*
   * Check side effects on target object.
   */

  ASSERT_EQ(1, memory().get_generation())
    << "target object should have its keys revoked";
  ASSERT_TRUE(memory().is_memory())
    << "target object should still be memory";
  ASSERT_EQ(p2range_under_test().base(), memory().get_range().base())
    << "target object base address should be unchanged";
  ASSERT_EQ(p2range_under_test().l2_half_size(),
            memory().get_range().l2_size())
    << "target object size should be reduced by half";

  /*
   * Check side effects on new (donated) object.
   */

  auto & newobj = _entries[3].as_object();
  ASSERT_EQ(1, newobj.get_generation())
    << "new object should have its keys revoked";
  ASSERT_TRUE(newobj.is_memory())
    << "new object should be memory";

  auto & newmem = static_cast<Memory &>(newobj);
  ASSERT_EQ(p2range_under_test().base() + p2range_under_test().half_size(),
            newmem.get_range().base())
    << "new object base address should be halfway into our range";
  ASSERT_EQ(p2range_under_test().l2_half_size(),
            newmem.get_range().l2_size())
    << "new object size should be reduced by half";

  /*
   * Check shape of bottom key.
   */

  auto & bk = spy.keys().keys[1];
  ASSERT_EQ(memory().get_generation(), bk.get_generation())
    << "returned bottom key must have right generation";
  ASSERT_EQ(&memory(), bk.get())
    << "returned bottom key must point to object under test";
  ASSERT_EQ(brand, bk.get_brand())
    << "returned bottom key's brand must match original";

  /*
   * Check shape of top key.
   */

  auto & tk = spy.keys().keys[2];
  ASSERT_EQ(newmem.get_generation(), tk.get_generation())
    << "returned top key must have right generation";
  ASSERT_EQ(&newmem, tk.get())
    << "returned top key must point to new object";
  ASSERT_EQ(brand, tk.get_brand())
    << "returned top key's brand must match original";
}


/*******************************************************************************
 * Tests at the 128-byte level, where subregions stop working.
 */

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

/*******************************************************************************
 * Tests that are specifically relevant for objects of the minimum size (32
 * bytes).
 */

class MemoryTest_Tiny : public MemoryTest {
protected:
  P2Range p2range_under_test() override {
    return P2Range::of(256, 4).ref();
  }
};

TEST_F(MemoryTest_Tiny, split_ok) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_write);
  auto brand = uint32_t(initial_rasr) >> 8;

  Spy spy{0};
  ReplySender sender{{Descriptor::call(2, 0)}};
  sender.set_key(0, spy.make_key(0).ref());
  sender.set_key(1, _entries[3].as_object().make_key(0).ref());

  memory().deliver_from(brand, &sender);

  ASSERT_EQ(1, spy.count()) << "single reply should be sent";
  auto & m = spy.message().m;
  ASSERT_TRUE(m.d0.get_error())
    << "splitting a 32-byte region should fail";
  ASSERT_EQ(uint64_t(Exception::bad_operation), (uint64_t(m.d2) << 32) | m.d1)
    << "exception should be bad_operation";
}


}  // namespace k

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
