#include "k/object_table.h"

#include "etl/error/check.h"
#include "etl/error/ignore.h"

#include "k/context.h"
#include "k/ipc.h"
#include "k/reply_sender.h"
#include "k/unprivileged.h"

namespace k {

ObjectTable object_table;

SysResult ObjectTable::deliver_from(uint32_t brand, Sender * sender) {
  Message m = CHECK(sender->get_message());
  switch (m.data[0]) {
    case 0: return mint_key(brand, sender, m);
    case 1: return read_key(brand, sender, m);
    
    default:
      return SysResult::bad_message;
  }
}

SysResult ObjectTable::mint_key(uint32_t,
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
  reply_sender.set_key(0, Key::filled(index, brand));
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

SysResult ObjectTable::read_key(uint32_t,
                                Sender * sender,
                                Message const & args) {
  auto k = sender->get_message_key(1);
  auto index = k.get_index();
  auto brand = k.get_brand();

  auto reply = sender->get_message_key(0);

  sender->complete_send();

  // TODO: systematic reply priorities?
  ReplySender reply_sender{0, {index, brand}};
  IGNORE(reply.deliver_from(&reply_sender));
  return SysResult::success;
}

}  // namespace k
