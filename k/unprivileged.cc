#include "k/unprivileged.h"

using etl::armv7m::Byte;
using etl::armv7m::Word;

namespace k {

uintptr_t mm_fault_recovery_handler;

/*
 * Loads
 */

bool uload_impl(Byte const *, Byte * out);
bool uload_impl(Word const *, Word * out);

template <typename T>
SysResultWith<T> uload_common(T const * p) {
  T result;
  if (uload_impl(p, &result)) {
    return {etl::error::left, SysResult::fault};
  } else {
    return {etl::error::right, result};
  }
}

SysResultWith<Word> uload(Word const * p) {
  return uload_common(p);
}

SysResultWith<Byte> uload(Byte const * p) {
  return uload_common(p);
}

/*
 * Stores
 */

bool ustore_impl(Byte *, Byte);
bool ustore_impl(Word *, Word);

template <typename T>
SysResult ustore_common(T * addr, T value) {
  if (ustore_impl(addr, value)) {
    return SysResult::fault;
  } else {
    return SysResult::success;
  }
}

SysResult ustore(Word * addr, Word value) {
  return ustore_common(addr, value);
}

SysResult ustore(Byte * addr, Byte value) {
  return ustore_common(addr, value);
}

}  // namespace k
