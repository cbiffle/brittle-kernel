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
 *
 * Memory objects fulfilling certain restrictions (power of two in size,
 * naturally aligned, 32 bytes or larger) are "mappable," meaning they can be
 * loaded into the MPU to allow a program direct access to address space.
 */

#include <cstddef>

#include "k/key.h"
#include "k/object.h"
#include "k/range_ptr.h"

namespace k {

struct Keys;  // see: k/keys.h
struct Region;  // see: k/region.h
struct ScopedReplySender;  // see: k/reply_sender.h

class Memory final : public Object {
public:
  /*
   * Masks for manipulating the attributes word.  This hasn't become complex
   * enough for me to provide safe accessors yet.
   */
  static constexpr uint32_t
    // Memory contains devices.
    device_attribute_mask = 1 << 0,
    // Memory is mappable.
    mappable_attribute_mask = 1 << 1,
    // Mask of the attributes exposed by Inspect.
    abi_attributes_mask = 0x3,
    // Bit offset to cached log2(half size) for mappable memory.
    cached_l2hs_lsb = 8,
    // Mask for cached log2(half size).
    cached_l2hs_mask = 0x1F << cached_l2hs_lsb;

  /*
   * Creates a Memory object of a certain Generation.
   *
   * 'base' and 'size' describe the extent of address space controlled by this
   * object.  'size' can be zero.
   *
   * 'attributes' is currently only used to propagate the 'device' property
   * from object to object during split or descent.  TODO: it should likely be
   * replaced by a bool.
   *
   * 'parent', when provided, makes the new Memory object a child of the given
   * existing object.
   *
   * Precondition: 'base+size' does not overflow a 32-bit unsigned integer.
   *
   * Precondition: the range of addresses is a (non-strict) subset of those
   * held by the 'parent', when provided.
   */
  Memory(Generation, uintptr_t base, size_t size, uint32_t attributes,
         Memory * parent = nullptr);

  /*
   * Destroys a Memory object, notifying any parent.
   */
  ~Memory();

  /*
   * Gets the base address of this Memory's address space.
   */
  uintptr_t get_base() const { return _base; }
  
  /*
   * Gets the size of this Memory's address space, in bytes.
   */
  size_t get_size() const { return _size_bytes; }

  /*
   * Checks whether this Memory has the "device" attribute set.
   */
  bool is_device() const { return _attributes & device_attribute_mask; }

  /*
   * Checks whether this Memory has the "mappable" attribute set.
   */
  bool is_mappable() const { return _attributes & mappable_attribute_mask; }

  /*
   * For mappable Memory, retrieves the log2(size/2) computed at creation.
   * For other Memory, the result is undefined.
   */
  unsigned get_cached_l2_half_size() const {
    return (_attributes & cached_l2hs_mask) >> cached_l2hs_lsb;
  }

  /*
   * Checks whether this Memory is "top," i.e. has no parent.
   */
  bool is_top() const { return _parent == nullptr; }

  /*
   * Returns the parent of this object, or nullptr if this object is top.
   */
  Memory * parent() const { return _parent; }

  /*
   * Returns the number of children beneath this Memory object.  If zero, this
   * object is a leaf.
   */
  uint32_t child_count() const { return _child_count; }

  /*
   * Marks this object as device memory.  This is mostly intended for testing.
   */
  void mark_as_device() { _attributes |= device_attribute_mask; }

  /*****************************************************************
   * Implementation of Object
   */

  /*
   * Translates a key to this Memory (described by its brand) into the actual
   * settings that would be loaded into the MPU.
   */
  Region get_region_for_brand(Brand const &) const override;

  void deliver_from(Brand const &, Sender *) override;
  Kind get_kind() const override { return Kind::memory; }

private:
  uintptr_t _base;
  size_t _size_bytes;

  uint32_t _attributes;

  Memory * _parent;
  uint32_t _child_count;

  void do_split(ScopedReplySender &, Brand const &, Message const &, Keys &);
};

}  // namespace k

#endif  // K_MEMORY_H
