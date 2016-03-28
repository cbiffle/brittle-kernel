#include "k/null_object.h"

#include "k/sender.h"

namespace k {

void NullObject::deliver_from(Brand const &, Sender * sender) {
  sender->on_delivery_failed(Exception::bad_operation);
}

}  // namespace k
