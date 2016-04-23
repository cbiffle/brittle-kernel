#include "k/null_object.h"

#include "k/sender.h"

namespace k {

NullObject::NullObject(Generation g) : Object{g} {}

void NullObject::deliver_from(Brand, Sender * sender) {
  sender->on_delivery_failed(Exception::bad_operation);
}

}  // namespace k
