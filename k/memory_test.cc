#include <gtest/gtest.h>
#include <cstdlib>

#include "etl/armv7m/mpu.h"

#include "common/abi_sizes.h"

#include "k/context.h"
#include "k/gate.h"
#include "k/interrupt.h"
#include "k/irq_redirector.h"
#include "k/memory.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/p2range.h"
#include "k/region.h"
#include "k/reply_sender.h"
#include "k/scheduler.h"
#include "k/slot.h"

#include "k/testutil/spy.h"

namespace k {

using Rbar = Region::Rbar;
using Rasr = Region::Rasr;
using etl::armv7m::Mpu;

class MemoryTest : public ::testing::Test {
protected:
  static constexpr Rasr rw_rasr =
    Rasr().with_ap(Mpu::AccessPermissions::p_write_u_write);

  ObjectTable::Entry _entries[5];

  Context::Body _fake_context_body;
  Context _fake_context{0, _fake_context_body};

  Spy _spy{0, Object::Kind::context};
  ReplySender _sender;

  void SetUp() override {
    new (&_entries[0]) NullObject{0};

    {
      auto o = new(&_entries[1]) ObjectTable{0};
      set_object_table(o);
      o->set_entries(_entries);
    }

    new(&_entries[2]) Memory{0, p2range_under_test(), 0};

    new(&_entries[3]) Slot{0};
    new(&_entries[4]) Slot{0};

    current = &_fake_context;
  }

  void TearDown() override {
    current = nullptr;
    reset_object_table_for_test();
  }

  Memory & memory() {
    return *static_cast<Memory *>(&_entries[2].as_object());
  }

  Object & object() {
    return _entries[2].as_object();
  }

  Object & slot() {
    return _entries[3].as_object();
  }

  Object & slot2() {
    return _entries[4].as_object();
  }

  Brand brand_from_rasr(Rasr v) {
    return uint32_t(v) >> 8;
  }

  Message const & send_from_spy(Rasr rasr, Message m) {
    _sender.message() = m;
    _sender.set_key(0, _spy.make_key(0).ref());
    memory().deliver_from(brand_from_rasr(rasr), &_sender);

    EXPECT_EQ(1, _spy.count()) << "single reply should be sent";

    return _spy.message().m;
  }

  virtual P2Range p2range_under_test() = 0;
};

constexpr Rasr MemoryTest::rw_rasr;

#define ASSERT_MESSAGE_SUCCESS(__m) \
  ASSERT_EQ(0, uint32_t((__m).desc))

#define ASSERT_NULL_KEY(__k) \
  ASSERT_EQ(Object::Kind::null, (__k).get()->get_kind())

#define ASSERT_RETURNED_KEY_SHAPE(_obj, _brand, _index) \
{ \
  auto & __obj = (_obj); \
  auto & __key = _spy.keys().keys[_index]; \
  auto __brand = (_brand); \
  ASSERT_EQ(__obj.get_generation(), __key.get_generation()) \
    << "returned key " #_index " must have same generation as " #_obj; \
  ASSERT_EQ(&__obj, __key.get()) \
    << "returned key " #_index " must point to " #_obj; \
  ASSERT_EQ(__brand, __key.get_brand()) \
    << "returned key " #_index " must have brand " #_brand; \
}

#define ASSERT_RETURNED_KEY_NULL(__idx) \
  ASSERT_NULL_KEY(_spy.keys().keys[__idx])

#define ASSERT_ALL_RETURNED_KEYS_NULL() \
{ \
  ASSERT_RETURNED_KEY_NULL(0) << "operation must not return any key 0"; \
  ASSERT_RETURNED_KEY_NULL(1) << "operation must not return any key 1"; \
  ASSERT_RETURNED_KEY_NULL(2) << "operation must not return any key 2"; \
  ASSERT_RETURNED_KEY_NULL(3) << "operation must not return any key 3"; \
}


#define ASSERT_RETURNED_EXCEPTION(_m, _e) \
{ \
  auto & __m = (_m); \
  auto __e = (_e); \
  ASSERT_TRUE(__m.desc.get_error()) \
    << "operation should have failed"; \
  ASSERT_EQ(uint64_t(__e), (uint64_t(__m.d1) << 32) | __m.d0) \
    << "operation failed with wrong exception"; \
  ASSERT_ALL_RETURNED_KEYS_NULL(); \
}


/*******************************************************************************
 * Basic tests of construction and hierarchy.
 */

TEST(MemoryTest, ctor) {
  auto range = P2Range::of(256, 7).ref();

  Memory m{0, range, 0};
  ASSERT_TRUE(m.is_top());
  ASSERT_EQ(m.child_count(), 0);
}

TEST(MemoryTest, construct_biggest_child) {
  auto range = P2Range::of(256, 7).ref();

  Memory parent{0, range, 0};
  Memory child{0, range, 0, &parent};

  ASSERT_TRUE(parent.is_top());
  ASSERT_EQ(parent.child_count(), 1);

  ASSERT_FALSE(child.is_top());
  ASSERT_EQ(child.child_count(), 0);
}

TEST(MemoryTest, construct_top_child) {
  auto range = P2Range::of(256, 7).ref();
  auto top = range.top().ref();

  Memory parent{0, range, 0};
  Memory child{0, top, 0, &parent};

  ASSERT_TRUE(parent.is_top());
  ASSERT_EQ(parent.child_count(), 1);

  ASSERT_FALSE(child.is_top());
  ASSERT_EQ(child.child_count(), 0);
}

TEST(MemoryTest, construct_bottom_child) {
  auto range = P2Range::of(256, 7).ref();
  auto bottom = range.bottom().ref();

  Memory parent{0, range, 0};
  Memory child{0, bottom, 0, &parent};

  ASSERT_TRUE(parent.is_top());
  ASSERT_EQ(parent.child_count(), 1);

  ASSERT_FALSE(child.is_top());
  ASSERT_EQ(child.child_count(), 0);
}

TEST(MemoryTest, construct_bad_child) {
  auto range = P2Range::of(256, 7).ref();
  auto bad_range = P2Range::of(0, 8).ref();

  Memory parent{0, range, 0};
  ASSERT_THROW(
      Memory child(0, bad_range, 0, &parent),
      std::logic_error);

  ASSERT_TRUE(parent.is_top());
  ASSERT_EQ(parent.child_count(), 0);
}




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

/*
 * Basics
 */

TEST_F(MemoryTest_Typical, bad_selector) {
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(0xFFFF, 0)});
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}

TEST_F(MemoryTest_Typical, inspect) {
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(0, 0)});

  ASSERT_MESSAGE_SUCCESS(m)
    << "inspecting a key should always succeed";
  ASSERT_ALL_RETURNED_KEYS_NULL();

  auto rbar = Rbar(m.d0);

  ASSERT_EQ(p2range_under_test().base(),
            rbar.get_addr_27() << 5)
    << "Base address should be conveyed unchanged";
  ASSERT_FALSE(rbar.get_valid()) << "Region number valid field must not be set";
  ASSERT_EQ(0, rbar.get_region()) << "Region number field must not be set";

  auto rasr = Rasr(m.d1);
  auto expected_rasr = rw_rasr.with_size(7).with_enable(true);
  ASSERT_EQ(uint32_t(expected_rasr), uint32_t(rasr))
    << "Revealed RASR should be initial RASR but with size and enable set";
}

/*
 * Change
 */

TEST_F(MemoryTest_Typical, change_tex) {
  auto initial_rasr = rw_rasr.with_tex(0);
  auto target_rasr = initial_rasr.with_tex(3);

  auto & m = send_from_spy(initial_rasr, {
      Descriptor::call(1, 0),
      uint32_t(target_rasr),
      });

  ASSERT_MESSAGE_SUCCESS(m)
    << "altering TEX should always succeed";
  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(memory(), brand_from_rasr(target_rasr), 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);
}

TEST_F(MemoryTest_Typical, change_disable_subregion) {
  auto initial_rasr = rw_rasr.with_srd(0x01);
  auto target_rasr = initial_rasr.with_srd(0x81);

  auto & m = send_from_spy(initial_rasr, {
      Descriptor::call(1, 0),
      uint32_t(target_rasr),
      });

  ASSERT_MESSAGE_SUCCESS(m)
    << "disabling a new subregion in a typical sized object should succeed";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(memory(), brand_from_rasr(target_rasr), 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);
}

TEST_F(MemoryTest_Typical, change_enable_subregion) {
  auto initial_rasr = rw_rasr.with_srd(0x81);
  auto target_rasr = initial_rasr.with_srd(0x01);

  auto & m = send_from_spy(initial_rasr, {
      Descriptor::call(1, 0),
      uint32_t(target_rasr),
      });
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_argument);
}

TEST_F(MemoryTest_Typical, change_increase_access) {
  auto initial_rasr = Rasr()
      .with_ap(Mpu::AccessPermissions::p_write_u_read);

  auto target_rasr =
    initial_rasr.with_ap(Mpu::AccessPermissions::p_write_u_write);

  auto & m = send_from_spy(initial_rasr, {
      Descriptor::call(1, 0),
      uint32_t(target_rasr),
      });
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_argument);
}

/*
 * Split
 */

TEST_F(MemoryTest_Typical, split_without_donation) {
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(2, 0)});
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_kind);
}

TEST_F(MemoryTest_Typical, split_srd_fail) {
  auto initial_rasr = rw_rasr.with_srd(0x01);

  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(initial_rasr, {Descriptor::call(2, 0)});
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}

TEST_F(MemoryTest_Typical, split_parent_must_fail) {
  // Create a child.
  new(&slot2()) Memory{0, p2range_under_test(), 0, &memory()};

  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(2, 0)});
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}

TEST_F(MemoryTest_Typical, split_ok) {
  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(2, 0)});

  ASSERT_MESSAGE_SUCCESS(m)
    << "splitting a typical-size object with a valid slot donation"
       " should succeed";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_NULL(3);

  /*
   * Check side effects on target object.
   */

  ASSERT_EQ(1, memory().get_generation())
    << "target object should have its keys revoked";
  ASSERT_TRUE(dynamic_cast<Memory *>(&memory()))
    << "target object should still be memory";
  ASSERT_EQ(p2range_under_test().base(), memory().get_range().base())
    << "target object base address should be unchanged";
  ASSERT_EQ(p2range_under_test().l2_half_size(),
            memory().get_range().l2_size())
    << "target object size should be reduced by half";

  /*
   * Check side effects on new (donated) object.
   */

  auto & newobj = slot();
  ASSERT_EQ(1, newobj.get_generation())
    << "new object should have its keys revoked";
  ASSERT_TRUE(dynamic_cast<Memory *>(&newobj))
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

  ASSERT_RETURNED_KEY_SHAPE(memory(), brand_from_rasr(rw_rasr), 1);

  /*
   * Check shape of top key.
   */

  ASSERT_RETURNED_KEY_SHAPE(newmem, brand_from_rasr(rw_rasr), 2);
}

/*
 * Make Child
 */

TEST_F(MemoryTest_Typical, make_child_without_donation) {
  auto range = p2range_under_test();
  auto & m = send_from_spy(rw_rasr, {
      Descriptor::call(6, 0),
      range.base(),
      range.l2_half_size(),
      });
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_kind);
}

TEST_F(MemoryTest_Typical, make_child_srd_fail) {
  auto initial_rasr = rw_rasr.with_srd(0x01);
  auto range = p2range_under_test();

  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(initial_rasr, {
      Descriptor::call(6, 0),
      range.base(),
      range.l2_half_size(),
      });
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}

TEST_F(MemoryTest_Typical, make_child_bigger) {
  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(rw_rasr, {
      Descriptor::call(6, 0),
      256,
      8,
      });
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_argument);
}

TEST_F(MemoryTest_Typical, make_child_too_small) {
  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(rw_rasr, {
      Descriptor::call(6, 0),
      256,
      3,
      });
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_argument);
}

TEST_F(MemoryTest_Typical, make_child_small_but_wrong) {
  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(rw_rasr, {
      Descriptor::call(6, 0),
      512,
      5,
      });
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_argument);
}

TEST_F(MemoryTest_Typical, make_child_ok) {
  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(rw_rasr, {
      Descriptor::call(6, 0),
      384,
      6,
      });

  ASSERT_MESSAGE_SUCCESS(m);
  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);

  /*
   * Check side effects on parent.
   */

  ASSERT_EQ(0, memory().get_generation())
    << "keys to parent should remain valid";
  ASSERT_EQ(1, memory().child_count())
    << "parent should now have one child";

  /*
   * Check side effects on child.
   */

  auto & newobj = slot();
  ASSERT_EQ(1, newobj.get_generation())
    << "donated slot should have its keys revoked";
  ASSERT_TRUE(dynamic_cast<Memory *>(&newobj))
    << "donated slot should now be memory";

  auto & newmem = static_cast<Memory &>(newobj);
  ASSERT_EQ(384, newmem.get_range().base());
  ASSERT_EQ(6, newmem.get_range().l2_half_size());

  /*
   * Check shape of child key
   */

  ASSERT_RETURNED_KEY_SHAPE(slot(), brand_from_rasr(rw_rasr), 1);
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
  auto target_rasr = rw_rasr.with_srd(0x01);

  auto & m = send_from_spy(rw_rasr, {
      Descriptor::call(1, 0),
      uint32_t(target_rasr),
      });

  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_argument);
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

TEST_F(MemoryTest_Tiny, split_too_small) {
  _sender.set_key(1, slot().make_key(0).ref());
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(2, 0)});
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}


/*******************************************************************************
 * Testing Memory's ability to become other sorts of objects.
 */

template <unsigned L2Size>
class MemoryTest_Become : public MemoryTest {
protected:
  static constexpr unsigned l2_size = L2Size;
  static_assert(l2_size < 32, "l2_size is probably an actual size instead");
  static constexpr unsigned l2_half_size = l2_size - 1;

  void * buffer;
  Interrupt * irq_table[4];

  unsigned get_size() const {
    return 1u << l2_size;
  }

  void SetUp() override {
    buffer = aligned_alloc(get_size(), get_size());

    for (auto & p : irq_table) p = nullptr;
    set_irq_redirection_table(irq_table);
  
    MemoryTest::SetUp();
  }

  void TearDown() override {
    MemoryTest::TearDown();
    reset_irq_redirection_table_for_test();
  }

  P2Range p2range_under_test() override {
    return P2Range::of(reinterpret_cast<uint32_t>(buffer), l2_half_size).ref();
  }

  Message const & send_become(Rasr rasr, unsigned tc, unsigned x = 0) {
    return send_from_spy(rasr, {
        Descriptor::call(3, 0),
        tc,
        x,
        });
  }
};

using MemoryTest_BecomeContext = MemoryTest_Become<kabi::context_l2_size>;
TEST_F(MemoryTest_BecomeContext, ok) {
  auto & m = send_become(rw_rasr, 0);

  ASSERT_FALSE(m.desc.get_error())
    << "this memory should be able to become a context; exception: "
    << std::hex
    << m.d1 << m.d0
    << std::dec << " " << m.d2;

  ASSERT_EQ(1, object().get_generation())
    << "The consumed memory's generation should have advanced.";
  ASSERT_TRUE(dynamic_cast<Context *>(&object()))
    << "The consumed memory should be a context now.";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(object(), 0, 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);
}

TEST_F(MemoryTest_BecomeContext, device_fail) {
  memory().mark_as_device();

  auto & m = send_become(rw_rasr, 0);
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}

using MemoryTest_BecomeGate = MemoryTest_Become<kabi::gate_l2_size>;
TEST_F(MemoryTest_BecomeGate, ok) {
  auto & m = send_become(rw_rasr, 1);

  ASSERT_FALSE(m.desc.get_error())
    << "this memory should be able to become a gate; exception: "
    << std::hex
    << m.d1 << m.d0
    << std::dec << " " << m.d2;

  ASSERT_EQ(1, object().get_generation())
    << "The consumed memory's generation should have advanced.";
  ASSERT_TRUE(dynamic_cast<Gate *>(&object()))
    << "The consumed memory should be a gate now.";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(object(), 0, 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);
}

using MemoryTest_BecomeInterrupt = MemoryTest_Become<kabi::interrupt_l2_size>;
TEST_F(MemoryTest_BecomeInterrupt, ok) {
  static constexpr unsigned irq_number = 1;
  auto & m = send_become(rw_rasr, 3, irq_number);

  ASSERT_FALSE(m.desc.get_error())
    << "this memory should be able to become an interrupt; exception: "
    << std::hex
    << m.d1 << m.d0
    << std::dec << " " << m.d2;

  ASSERT_EQ(1, object().get_generation())
    << "The consumed memory's generation should have advanced.";
  ASSERT_TRUE(dynamic_cast<Interrupt *>(&object()))
    << "The consumed memory should be an interrupt now.";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(object(), 0, 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);

  // Note that table offset is 1+ interrupt number because of systick.
  ASSERT_EQ(&object(), irq_table[irq_number + 1])
    << "interrupt should have wired itself into the table";
}

}  // namespace k

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
