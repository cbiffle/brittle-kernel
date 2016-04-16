#include "k/object_table.h"

#include "common/exceptions.h"
#include "common/message.h"

#include "k/context.h"
#include "k/reply_sender.h"
#include "k/unprivileged.h"

namespace k {

static ObjectTable * instance;

ObjectTable & object_table() {
  ETL_ASSERT(instance);
  return *instance;
}

void set_object_table(ObjectTable * p) {
  ETL_ASSERT(instance == nullptr);
  ETL_ASSERT(p);
  instance = p;
}

void reset_object_table_for_test() {
  instance = nullptr;
}

void ObjectTable::set_entries(RangePtr<Entry> entries) {
  ETL_ASSERT(_objects.is_empty());
  _objects = entries;
}

void ObjectTable::invalidate(TableIndex index) {
  ++_objects[index].generation;
}

void ObjectTable::deliver_from(Brand const & brand, Sender * sender) {
  Keys k;
  Message m = sender->on_delivery_accepted(k);
  switch (m.d0.get_selector()) {
    case 0:
      do_mint_key(brand, m, k);
      break;

    case 1: 
      do_read_key(brand, m, k);
      break;
    
    default:
      do_badop(m, k);
      break;
  }
}

void ObjectTable::do_mint_key(Brand const &,
                              Message const & args,
                              Keys & keys) {
  auto index = args.d1;
  auto brand = args.d2 | (Brand(args.d3) << 32);

  ScopedReplySender reply_sender{keys.keys[0]};

  if (index >= _objects.count()) {
    reply_sender.get_message() = Message::failure(Exception::index_out_of_range);
  } else {
    // Give the recipient a chance to reject the brand.
    auto p = _objects[index].ptr ? _objects[index].ptr : _objects[0].ptr;
    if (auto maybe_key = p->make_key(brand)) {
      reply_sender.set_key(1, maybe_key.ref());
    } else {
      reply_sender.get_message() = Message::failure(Exception::bad_brand);
    }
  }
}

void ObjectTable::do_read_key(Brand const &,
                              Message const &,
                              Keys & keys) {
  auto & k = keys.keys[1];
  auto index = k.get_index();
  auto brand = k.get_brand();

  ScopedReplySender reply_sender{keys.keys[0], {
    Descriptor::zero(),
    index,
    uint32_t(brand),
    uint32_t(brand >> 32),
  }};
}

}  // namespace k
