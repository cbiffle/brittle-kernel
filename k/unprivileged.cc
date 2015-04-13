#include "k/unprivileged.h"

#include "etl/error/check.h"

using etl::armv7m::Byte;
using etl::armv7m::DoubleWord;
using etl::armv7m::Word;
using etl::data::Maybe;

namespace k {

uintptr_t mm_fault_recovery_handler;

/*
 * Loads
 */

bool uload_impl(Byte const *, Byte * out);
bool uload_impl(Word const *, Word * out);

template <typename T>
Maybe<T> uload_common(T const * p) {
  T result;
  if (uload_impl(p, &result)) {
    return etl::data::nothing;
  } else {
    return result;
  }
}

Maybe<Word> uload(Word const * p) {
  return uload_common(p);
}

Maybe<Byte> uload(Byte const * p) {
  return uload_common(p);
}

/*
 * Stores
 */

bool ustore_impl(Byte *, Byte);
bool ustore_impl(Word *, Word);

template <typename T>
bool ustore_common(T * addr, T value) {
  if (ustore_impl(addr, value)) {
    return false;
  } else {
    return true;
  }
}

bool ustore(Word * addr, Word value) {
  return ustore_common(addr, value);
}

bool ustore(Byte * addr, Byte value) {
  return ustore_common(addr, value);
}

bool ustore(DoubleWord * addr, DoubleWord value) {
  return ustore(reinterpret_cast<Word *>(addr), Word(value))
      && ustore(reinterpret_cast<Word *>(addr) + 1, Word(value >> 32));
}

}  // namespace k
