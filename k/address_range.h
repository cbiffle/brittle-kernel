#ifndef K_ADDRESS_RANGE_H
#define K_ADDRESS_RANGE_H

#include "k/key.h"
#include "k/object.h"
#include "k/range_ptr.h"

namespace k {

struct Keys;  // see: k/keys.h
struct Region;  // see: k/region.h

/*
 * An AddressRange describes a range of addresses and responds to the memory
 * region protocol.  Keys to the AddressRange specify the properties of any
 * memory region contained within its range.
 */
class AddressRange final : public Object {
public:
  enum class ReadOnly {
    no,
    unpriv,
    priv,
  };

  AddressRange(RangePtr<uint8_t>,
               bool prevent_execution,
               ReadOnly);

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
   * Overridden to insist that this is, in fact, an AddressRange.
   */
  virtual bool is_address_range() const override;

private:
  RangePtr<uint8_t> _range;
  bool _xn;
  ReadOnly _read_only;

  void do_inspect(Brand const &, Message const &, Keys &);
};

}  // namespace k

#endif  // K_ADDRESS_RANGE_H
