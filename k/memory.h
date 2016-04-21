#ifndef K_MEMORY_H
#define K_MEMORY_H

/*
 * A Memory object describes a range of addresses and responds to the memory
 * region protocol.  Keys to the Memory specify the properties of any memory
 * region contained within its range.
 */

#include "k/key.h"
#include "k/object.h"
#include "k/p2range.h"

namespace k {

struct Keys;  // see: k/keys.h
struct Region;  // see: k/region.h

class Memory final : public Object {
public:
  Memory(Generation, P2Range);

  P2Range get_range() const { return _range; }

  /*
   * Translates a key to this Memory (described by its brand) into the actual
   * settings that would be loaded into the MPU.
   */
  Region get_region_for_brand(Brand) const;

  void deliver_from(Brand const &, Sender *) override;

  /*
   * Overridden to insist that this is, in fact, Memory.
   */
  virtual bool is_memory() const override;

private:
  P2Range _range;

  void do_inspect(Brand const &, Message const &, Keys &);
  void do_change(Brand const &, Message const &, Keys &);
  void do_split(Brand const &, Message const &, Keys &);
  void do_become(Brand const &, Message const &, Keys &);
  void do_peek(Brand const &, Message const &, Keys &);
  void do_poke(Brand const &, Message const &, Keys &);
};

}  // namespace k

#endif  // K_MEMORY_H
