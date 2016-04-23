#include "k/slot.h"

#include "k/sender.h"

namespace k {

void Slot::deliver_from(Brand, Sender * sender) {
  sender->on_delivery_failed(Exception::bad_operation);
}

bool Slot::is_slot() const {
  return true;
}

}  // namespace k
