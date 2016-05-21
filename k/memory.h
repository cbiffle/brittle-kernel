#ifndef K_MEMORY_H
#define K_MEMORY_H

/*
 * A Memory object describes a range of addresses and responds to the memory
 * region protocol.  Keys to the Memory specify the properties of any memory
 * region contained within its range.
 *
 * Memory objects can be marked as "device" memory.  Device memory behaves the
 * same as normal memory in nearly all circumstances, but *cannot* be donated
 * to the kernel using Become.
 */

#include "k/key.h"
#include "k/object.h"
#include "k/p2range.h"

namespace k {

struct Keys;  // see: k/keys.h
struct Region;  // see: k/region.h
struct ScopedReplySender;  // see: k/reply_sender.h

class Memory final : public Object {
public:
  static constexpr uint32_t device_attribute_mask = 1 << 0;

  Memory(Generation, P2Range, uint32_t attributes);

  P2Range get_range() const { return _range; }
  bool is_device() const { return _attributes & device_attribute_mask; }

  /*
   * Marks this object as device memory.  This is mostly intended for testing.
   */
  void mark_as_device() { _attributes |= device_attribute_mask; }

  /*
   * Translates a key to this Memory (described by its brand) into the actual
   * settings that would be loaded into the MPU.
   */
  Region get_region_for_brand(Brand const &) const override;

  void deliver_from(Brand const &, Sender *) override;
  Kind get_kind() const override { return Kind::memory; }

private:
  P2Range _range;
  uint32_t _attributes;

  void do_split(ScopedReplySender &, Brand const &, Message const &, Keys &);
};

}  // namespace k

#endif  // K_MEMORY_H
