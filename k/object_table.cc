#include "k/object_table.h"

#include "etl/error/check.h"
#include "etl/error/ignore.h"

#include "k/context.h"
#include "k/ipc.h"
#include "k/reply_sender.h"
#include "k/unprivileged.h"

namespace k {

void ObjectTable::invalidate(TableIndex index) {
  ETL_ASSERT(index < config::n_objects);

  ++_objects[index].generation;
}

SysResult ObjectTable::deliver_from(Brand brand, Sender * sender) {
  Message m = CHECK(sender->get_message());
  switch (m.data[0]) {
    case 0: return mint_key(brand, sender, m);
    case 1: return read_key(brand, sender, m);
    
    default:
      return SysResult::bad_message;
  }
}

SysResult ObjectTable::mint_key(Brand,
                                Sender * sender,
                                Message const & args) {
  auto index = args.data[1];
  auto brand = args.data[2];
  auto reply = sender->get_message_key(0);

  if (index >= config::n_objects) {
    return SysResult::bad_message;
  }

  sender->complete_send();

  ReplySender reply_sender{0};  // TODO: systematic reply priorities?

  // Give the recipient a chance to reject the brand.
  if (_objects[index].ptr) {
    auto maybe_key = _objects[index].ptr->make_key(brand);
    if (maybe_key) {
      reply_sender.set_message({1});
      reply_sender.set_key(0, maybe_key.ref());
    }
  }
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult ObjectTable::read_key(Brand,
                                Sender * sender,
                                Message const & args) {
  auto k = sender->get_message_key(1);
  auto index = k.get_index();
  auto brand = k.get_brand();

  auto reply = sender->get_message_key(0);

  sender->complete_send();

  // TODO: systematic reply priorities?
  ReplySender reply_sender{0, {
    index,
    uint32_t(brand),
    uint32_t(brand >> 32),
  }};
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

}  // namespace k
