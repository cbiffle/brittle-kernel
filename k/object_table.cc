#include "k/object_table.h"

#include "common/exceptions.h"
#include "common/message.h"
#include "common/selectors.h"

#include "k/context.h"
#include "k/panic.h"
#include "k/reply_sender.h"

namespace k {

static ObjectTable * instance;

ObjectTable & object_table() {
  PANIC_UNLESS(instance, "ObjectTable singleton not set");
  return *instance;
}

void set_object_table(ObjectTable * p) {
  PANIC_IF(instance, "ObjectTable singleton set twice");
  PANIC_UNLESS(p, "ObjectTable singleton set to nullptr");
  instance = p;
}

void reset_object_table_for_test() {
  instance = nullptr;
}

ObjectTable::ObjectTable(Generation g) : Object{g} {}

void ObjectTable::set_entries(RangePtr<Entry> entries) {
  PANIC_IF(entries.is_empty(), "ObjectTable entries set to empty range");
  PANIC_UNLESS(_objects.is_empty(), "ObjectTable entries set twice");
  _objects = entries;
}

void ObjectTable::deliver_from(Brand const & brand, Sender * sender) {
  Keys k;
  Message m = sender->on_delivery(k);

  namespace S = selector::object_table;
  switch (m.desc.get_selector()) {
    case S::mint_key:
      do_mint_key(brand, m, k);
      break;

    case S::read_key:
      do_read_key(brand, m, k);
      break;

    case S::get_kind:
      do_get_kind(brand, m, k);
      break;

    case S::invalidate:
      do_invalidate(brand, m, k);
      break;
    
    default:
      do_badop(m, k);
      break;
  }
}

void ObjectTable::do_mint_key(Brand const &,
                              Message const & args,
                              Keys & keys) {
  auto index = args.d0;
  auto brand = args.d1 | (Brand(args.d2) << 32);

  ScopedReplySender reply_sender{keys.keys[0]};

  if (index >= _objects.count()) {
    reply_sender.message() = Message::failure(Exception::index_out_of_range);
  } else {
    // Give the recipient a chance to reject the brand.
    if (auto maybe_key = _objects[index].as_object().make_key(brand)) {
      reply_sender.set_key(1, maybe_key.ref());
    } else {
      reply_sender.message() = Message::failure(Exception::bad_brand);
    }
  }
}

void ObjectTable::do_read_key(Brand const &,
                              Message const &,
                              Keys & keys) {
  auto & k = keys.keys[1];
  auto index = uint32_t(reinterpret_cast<Entry *>(k.get()) - _objects.base());
  auto brand = k.get_brand();

  ScopedReplySender reply_sender{keys.keys[0], {
    Descriptor::zero(),
    index,
    uint32_t(brand),
    uint32_t(brand >> 32),
  }};
}

void ObjectTable::do_get_kind(Brand const &,
                              Message const & args,
                              Keys & keys) {
  auto index = args.d0;

  ScopedReplySender reply_sender{keys.keys[0]};

  auto & reply = reply_sender.message();
  if (index >= _objects.count()) {
    reply = Message::failure(Exception::index_out_of_range);
  } else {
    reply.d0 = uint32_t(_objects[index].as_object().get_kind());
  }
}

void ObjectTable::do_invalidate(Brand const &,
                                Message const & args,
                                Keys & keys) {
  auto index = args.d0;
  auto rollover_ok = bool(args.d1);

  ScopedReplySender reply_sender{keys.keys[0]};

  if (index >= _objects.count()) {
    reply_sender.message() = Message::failure(Exception::index_out_of_range);
    return;
  }

  auto & obj = _objects[index].as_object();
  if (!rollover_ok && obj.get_generation() == Generation(0) - 1) {
    reply_sender.message() = Message::failure(Exception::causality);
    return;
  }

  obj.invalidate();
}

}  // namespace k
