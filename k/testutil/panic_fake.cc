#include "k/panic.h"
#include <stdexcept>

namespace k {

void panic(char const * message) {
  throw std::logic_error(message);
}

}  // namespace k
