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
#include "k/reply_gate.h"
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

  ObjectTable::Entry _entries[4];

  Context::Body _fake_context_body;
  Context _fake_context{0, _fake_context_body};

  ReplyGate::Body _fake_reply_gate_body;
  ReplyGate _fake_reply_gate{0, _fake_reply_gate_body};

  Spy _spy{0, Object::Kind::reply_gate};
  ReplySender _sender;

  void SetUp() override {
    new (&_entries[0]) NullObject{0};

    {
      auto o = new(&_entries[1]) ObjectTable{0};
      set_object_table(o);
      o->set_entries(_entries);
    }

    new(&_entries[2]) Memory{0, p2range_under_test()};

    new(&_entries[3]) Slot{0};

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

  Brand brand_from_rasr(Rasr v) {
    return uint32_t(v) >> 8;
  }

  Message const & send_from_spy(Rasr rasr, Message m) {
    _sender.get_message() = m;
    _sender.set_key(0, _spy.make_key(0).ref());
    memory().deliver_from(brand_from_rasr(rasr), &_sender);

    EXPECT_EQ(1, _spy.count()) << "single reply should be sent";

    return _spy.message().m;
  }

  virtual P2Range p2range_under_test() = 0;
};

constexpr Rasr MemoryTest::rw_rasr;

#define ASSERT_MESSAGE_SUCCESS(__m) \
  ASSERT_EQ(0, uint32_t((__m).d0))

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
  ASSERT_TRUE(__m.d0.get_error()) \
    << "operation should have failed"; \
  ASSERT_EQ(uint64_t(__e), (uint64_t(__m.d2) << 32) | __m.d1) \
    << "operation failed with wrong exception"; \
  ASSERT_ALL_RETURNED_KEYS_NULL(); \
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

TEST_F(MemoryTest_Typical, bad_selector) {
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(0xFFFF, 0)});
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}

TEST_F(MemoryTest_Typical, inspect) {
  auto & m = send_from_spy(rw_rasr, {Descriptor::call(0, 0)});

  ASSERT_MESSAGE_SUCCESS(m)
    << "inspecting a key should always succeed";
  ASSERT_ALL_RETURNED_KEYS_NULL();

  auto rbar = Rbar(m.d1);

  ASSERT_EQ(p2range_under_test().base(),
            rbar.get_addr_27() << 5)
    << "Base address should be conveyed unchanged";
  ASSERT_FALSE(rbar.get_valid()) << "Region number valid field must not be set";
  ASSERT_EQ(0, rbar.get_region()) << "Region number field must not be set";

  auto rasr = Rasr(m.d2);
  auto expected_rasr = rw_rasr.with_size(7).with_enable(true);
  ASSERT_EQ(uint32_t(expected_rasr), uint32_t(rasr))
    << "Revealed RASR should be initial RASR but with size and enable set";
}

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
  _sender.set_key(1, _fake_reply_gate.make_key(0).ref());
  auto & m = send_become(rw_rasr, 0);

  ASSERT_FALSE(m.d0.get_error())
    << "this memory should be able to become a context; exception: "
    << std::hex
    << m.d2 << m.d1
    << std::dec << " " << m.d3;

  ASSERT_EQ(1, object().get_generation())
    << "The consumed memory's generation should have advanced.";
  ASSERT_TRUE(dynamic_cast<Context *>(&object()))
    << "The consumed memory should be a context now.";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(object(), 0, 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);
}

using MemoryTest_BecomeGate = MemoryTest_Become<kabi::gate_l2_size>;
TEST_F(MemoryTest_BecomeGate, ok) {
  auto & m = send_become(rw_rasr, 1);

  ASSERT_FALSE(m.d0.get_error())
    << "this memory should be able to become a gate; exception: "
    << std::hex
    << m.d2 << m.d1
    << std::dec << " " << m.d3;

  ASSERT_EQ(1, object().get_generation())
    << "The consumed memory's generation should have advanced.";
  ASSERT_TRUE(dynamic_cast<Gate *>(&object()))
    << "The consumed memory should be a gate now.";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(object(), 0, 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);
}

using MemoryTest_BecomeReplyGate = MemoryTest_Become<kabi::reply_gate_l2_size>;
TEST_F(MemoryTest_BecomeReplyGate, ok) {
  auto & m = send_become(rw_rasr, 2);

  ASSERT_FALSE(m.d0.get_error())
    << "this memory should be able to become a reply gate; exception: "
    << std::hex
    << m.d2 << m.d1
    << std::dec << " " << m.d3;

  ASSERT_EQ(1, object().get_generation())
    << "The consumed memory's generation should have advanced.";
  ASSERT_TRUE(dynamic_cast<ReplyGate *>(&object()))
    << "The consumed memory should be a reply gate now.";

  ASSERT_RETURNED_KEY_NULL(0);
  ASSERT_RETURNED_KEY_SHAPE(object(), 0, 1);
  ASSERT_RETURNED_KEY_NULL(2);
  ASSERT_RETURNED_KEY_NULL(3);
}

using MemoryTest_BecomeInterrupt = MemoryTest_Become<kabi::interrupt_l2_size>;
TEST_F(MemoryTest_BecomeInterrupt, ok) {
  static constexpr unsigned irq_number = 1;
  auto & m = send_become(rw_rasr, 3, irq_number);

  ASSERT_FALSE(m.d0.get_error())
    << "this memory should be able to become an interrupt; exception: "
    << std::hex
    << m.d2 << m.d1
    << std::dec << " " << m.d3;

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
