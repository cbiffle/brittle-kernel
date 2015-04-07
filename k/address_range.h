#ifndef K_ADDRESS_RANGE_H
#define K_ADDRESS_RANGE_H

#include "etl/data/range_ptr.h"

#include "k/object.h"
#include "k/region.h"

namespace k {

/*
 * An AddressRange describes a range of addresses and responds to the memory
 * region protocol.  Keys to the AddressRange specify the properties of any
 * memory region contained within its range.
 */
class AddressRange : public Object {
public:
  enum class ReadOnly {
    no,
    unpriv,
    priv,
  };

  AddressRange(etl::data::RangePtr<uint8_t>,
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
  etl::data::Maybe<Key> make_key(Brand) override;

  void deliver_from(Brand, Sender *) override;

  /*
   * Overridden to insist that this is, in fact, an AddressRange.
   */
  virtual bool is_address_range() const;

private:
  etl::data::RangePtr<uint8_t> _range;
  bool _xn;
  ReadOnly _read_only;

  void inspect(Brand, Sender *, Message const &);
};

}  // namespace k

#endif  // K_ADDRESS_RANGE_H
