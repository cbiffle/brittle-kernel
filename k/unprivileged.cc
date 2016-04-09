#include "k/unprivileged.h"

using etl::armv7m::Byte;
using etl::armv7m::Word;

namespace k {

uintptr_t mm_fault_recovery_handler;

/*
 * Loads
 */

// These are defined in unprivileged.S.
bool uload_impl(Byte const *, Byte * out);
bool uload_impl(Word const *, Word * out);

template <typename T>
Maybe<T> uload_common(T const * p) {
  T result;
  if (uload_impl(p, &result)) {
    return nothing;
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

// These are defined in unprivileged.S.
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

}  // namespace k
