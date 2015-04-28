#include "k/null_object.h"

#include "k/sender.h"

namespace k {

void NullObject::deliver_from(Brand, Sender * sender) {
  // TODO: should a null object refuse delivery like this?
  sender->on_delivery_failed(Exception::bad_operation);
}

}  // namespace k
