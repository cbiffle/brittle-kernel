#ifndef K_MEMORY_H
#define K_MEMORY_H

#include "k/key.h"
#include "k/object.h"
#include "k/range_ptr.h"

namespace k {

struct Keys;  // see: k/keys.h
struct Region;  // see: k/region.h

/*
 * A Memory object describes a range of addresses and responds to the memory
 * region protocol.  Keys to the Memory specify the properties of any memory
 * region contained within its range.
 */
class Memory final : public Object {
public:
  Memory(RangePtr<uint8_t>);

  Region get_region_for_brand(Brand) const;
  static Brand get_brand_for_region(Region);

  void * get_region_begin(Brand) const;
  void * get_region_end(Brand) const;

  /*
   * Overridden to reject brands that would escalate privileges or leak outside
   * our range.
   */
  Maybe<Key> make_key(Brand) override;

  void deliver_from(Brand const &, Sender *) override;

  /*
   * Overridden to insist that this is, in fact, Memory.
   */
  virtual bool is_memory() const override;

private:
  RangePtr<uint8_t> _range;

  void do_inspect(Brand const &, Message const &, Keys &);
};

}  // namespace k

#endif  // K_MEMORY_H
