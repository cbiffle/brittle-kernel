#ifndef K_OBJECT_H
#define K_OBJECT_H

#include <stdint.h>

#include "etl/data/maybe.h"

#include "k/key.h"
#include "k/sys_result.h"
#include "k/types.h"

namespace k {

struct Context;  // see: k/context.h
struct Message;  // see: k/ipc.h
struct Sender;   // see: k/sender.h

class Object {
public:
  /*
   * Sets the object table index for this object, so that the object can find
   * itself and make keys.
   */
  void set_index(TableIndex index) { _index = index; }

  /*
   * Gets the objet table index for this object.
   */
  TableIndex get_index() const { return _index; }

  /*
   * Generates a key to this object with the given brand, if the brand is
   * acceptable for this object.  The default implementation accepts all
   * brands.  Subclasses can implement this to be more selective.
   */
  virtual etl::data::Maybe<Key> make_key(Brand);

  /*
   * Delivers a message from 'sender' to this object, through a key bearing
   * 'brand'.
   *
   * If this object can reason about the send right away, it should:
   * - Use sender's member functions to collect information about the message.
   * - Call sender's complete_send function with a SysResult for the send
   *   phase.
   * - If doing a call-style invocation, generate a reply to k0 from the
   *   message.
   *
   * This object may not be able to accept the message right away, in which
   * case it should ask the sender to block by calling block_in_send.  The
   * sender may refuse this, ending the process.
   *
   * This object can inspect the sender's message using member functions
   * while the sender is blocked.  Once it decides to unblock the sender and
   * take action on the message, it should call complete_blocked_send in
   * place of complete_send and finish the process above.
   *
   * The return value from this function indicates the status of the original
   * send.  If a send is blocked, the status is still 'success'.  A failure
   * to e.g. access parameters at the addresses that the sender passed would
   * result in failure.
   */
  virtual SysResult deliver_from(Brand, Sender *) = 0;

  /*
   * The given Context wants to receive a message from this object.  This
   * operation is typically valid only on gates, but is implemented uniformly
   * on all objects to avoid the need for a cast.
   *
   * The default implementation returns SysResult::bad_key.
   */
  virtual SysResult deliver_to(Brand, Context *);

  /*
   * Checks whether this Object is really an AddressRange.  The default
   * implementation returns false.
   */
  virtual bool is_address_range() const;

protected:
  Object();

private:
  TableIndex _index;
};

}  // namespace k

#endif  // K_OBJECT_H
