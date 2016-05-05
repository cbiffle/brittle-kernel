#include "k/testutil/spy.h"

#include "k/sender.h"

namespace k {

void Spy::deliver_from(Brand const & brand, Sender * sender) {
  ++_count;

  _received.brand = brand;
  _received.m = sender->on_delivery(_keys);
}

}  // namespace k
