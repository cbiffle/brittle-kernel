#include "k/unprivileged.h"

using etl::armv7m::Byte;
using etl::armv7m::Word;

namespace k {

Maybe<Word> uload(Word const * p) {
  return *p;
}

Maybe<Byte> uload(Byte const * p) {
  return *p;
}

bool ustore(Word * addr, Word value) {
  *addr = value;
  return true;
}

bool ustore(Byte * addr, Byte value) {
  *addr = value;
  return true;
}

}  // namespace k
