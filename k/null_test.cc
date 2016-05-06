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

class NullTest : public ::testing::Test {
protected:
  ObjectTable::Entry _entries[2];

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

    current = &_fake_context;
  }

  void TearDown() override {
    current = nullptr;
    reset_object_table_for_test();
  }

  NullObject & null() {
    return *static_cast<NullObject *>(&_entries[0].as_object());
  }

  Message const & send_from_spy(Message m) {
    _sender.message() = m;
    _sender.set_key(0, _spy.make_key(0).ref());
    null().deliver_from(0, &_sender);

    EXPECT_EQ(1, _spy.count()) << "single reply should be sent";

    return _spy.message().m;
  }
};

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

TEST_F(NullTest, basic) {
  /*
   * Here the spy looks like a Context, so Null should respond.
   */
  auto & m = send_from_spy({Descriptor::call(0, 0)});
  ASSERT_RETURNED_EXCEPTION(m, Exception::bad_operation);
}

TEST_F(NullTest, no_loop) {
  /*
   * Here the spy looks like Null itself, so Null should not respond.
   */
  _sender.message() = {Descriptor::call(0, 0)};
  _sender.set_key(0, _spy.make_key(0).ref());
  _spy.set_kind(Object::Kind::null);

  null().deliver_from(0, &_sender);

  ASSERT_EQ(0, _spy.count()) << "Null must not respond to itself.";
}

}  // namespace k

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
