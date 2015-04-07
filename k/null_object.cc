#include "k/null_object.h"

#include "k/sender.h"

namespace k {

void NullObject::deliver_from(Brand, Sender * sender) {
  sender->complete_send(Exception::bad_operation);
}

}  // namespace k
