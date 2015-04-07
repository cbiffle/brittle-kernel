#include "k/object_table.h"

#include "k/context.h"
#include "k/exceptions.h"
#include "k/ipc.h"
#include "k/reply_sender.h"
#include "k/unprivileged.h"

namespace k {

void ObjectTable::invalidate(TableIndex index) {
  ETL_ASSERT(index < config::n_objects);

  ++_objects[index].generation;
}

void ObjectTable::deliver_from(Brand brand, Sender * sender) {
  Message m = sender->get_message();
  switch (m.d0.get_selector()) {
    case 0:
      mint_key(brand, sender, m);
      break;

    case 1: 
      read_key(brand, sender, m);
      break;
    
    default:
      sender->complete_send(Exception::bad_operation, m.d0.get_selector());
      break;
  }
}

void ObjectTable::mint_key(Brand,
                           Sender * sender,
                           Message const & args) {
  auto index = args.d1;
  auto brand = args.d2 | (Brand(args.d3) << 32);
  auto reply = sender->get_message_key(0);

  sender->complete_send();

  ReplySender reply_sender{0};  // TODO: systematic reply priorities?

  if (index >= config::n_objects) {
    reply_sender.set_message(Message::failure(Exception::index_out_of_range));
  } else {
    // Give the recipient a chance to reject the brand.
    auto p = _objects[index].ptr ? _objects[index].ptr : _objects[0].ptr;
    auto maybe_key = p->make_key(brand);
    if (!maybe_key) {
      reply_sender.set_message(Message::failure(Exception::bad_brand));
    } else {
      reply_sender.set_message({});
      reply_sender.set_key(1, maybe_key.ref());
    }
  }

  reply.deliver_from(&reply_sender);
}

void ObjectTable::read_key(Brand,
                           Sender * sender,
                           Message const & args) {
  auto k = sender->get_message_key(1);
  auto index = k.get_index();
  auto brand = k.get_brand();

  auto reply = sender->get_message_key(0);

  sender->complete_send();

  // TODO: systematic reply priorities?
  ReplySender reply_sender{0, {
    Descriptor::zero(),
    index,
    uint32_t(brand),
    uint32_t(brand >> 32),
  }};
  reply.deliver_from(&reply_sender);
}

}  // namespace k
