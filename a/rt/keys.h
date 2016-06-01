#ifndef RT_KEYS_H
#define RT_KEYS_H

namespace rt {

/*
 * Marks a key register as being unavailable for automatic allocation.  This
 * is useful if a Context is handed some keys on startup.
 */
void reserve_key(unsigned);

/*
 * Marks a set of key registers as being unavailable for automatic allocation.
 * This is useful if a Context is handed some keys on startup.  The key
 * registers are expressed as a bitmask, where each bit (starting at the LSB)
 * corresponds to a key register.
 */
void reserve_keys(unsigned);

/*
 * RAII-style object for dynamically reserving a key register.
 */
class AutoKey {
public:
  /*
   * Reserves a key register.  If all key registers are taken, asserts.
   */
  AutoKey();

  /*
   * Moves a key register reservation from one object to another.  This is
   * intended to support assignment and return of AutoKey instances.
   */
  AutoKey(AutoKey &&);
  AutoKey(AutoKey const &) = delete;

  /*
   * Frees a key register.
   */
  ~AutoKey();

  /*
   * Convenience operator for converting to a register index.
   */
  operator unsigned() const {
    return _ki;
  }

private:
  unsigned _ki;
};

}  // namespace sys

#endif  // RT_KEYS_H
