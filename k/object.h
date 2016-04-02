#ifndef K_OBJECT_H
#define K_OBJECT_H

#include <stdint.h>

#include "common/abi_types.h"
#include "common/message.h"

#include "k/key.h"
#include "k/list.h"
#include "k/maybe.h"

namespace k {

struct Context;    // see: k/context.h
struct Keys;       // see: k/keys.h
struct Sender;     // see: k/sender.h

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
  virtual Maybe<Key> make_key(Brand);

  /*
   * Delivers a message from 'sender' to this object, through a key bearing
   * 'brand'.
   *
   * If this object can reason about the send right away, it should:
   * - Use sender's member functions to collect information about the message.
   * - Call the sender's complete_send function to end the send phase.
   * - If doing a call-style invocation, generate a reply to sender's k0.
   *
   * This object may not be able to accept the message right away, in which
   * case it should ask the sender to block by calling block_in_send.  The
   * sender may or may not agree to do so, but that's between the sender and
   * the provided list.
   *
   * This object can inspect the sender's message using member functions
   * while the sender is blocked.  Once it decides to unblock the sender and
   * take action on the message, it should call complete_blocked_send in place
   * of normal complete_send and finish the process above.
   */
  virtual void deliver_from(Brand const &, Sender *) = 0;

  /*
   * The given Context wants to receive a message from this object.  This
   * operation is typically valid only on gates, but is implemented uniformly
   * on all objects to avoid the need for a cast.
   *
   * If this object has something to say right now, it should:
   * - Call the context's complete_receive member function, providing a
   *   brand and sender.
   * 
   * If the object may have something to say later but needs to block the
   * context, it should:
   * - Call the context's block_in_receive function.  The context may or may
   *   not actually block itself, depending on the code controlling it.
   * - Later, on finding a context on a list, use complete_blocked_receive to
   *   deliver the message.
   *
   * If receiving from this object is not appropriate (the default
   * implementation), call the context's complete_blocked_receive member
   * function with an exception.
   *
   * The default implementation fails with a bad_operation Exception.
   */
  virtual void deliver_to(Context *);

  /*
   * Checks whether this Object is really an AddressRange.  The default
   * implementation returns false.
   */
  virtual bool is_address_range() const;

  /*
   * Checks whether this Object is really a Gate.  The default implementation
   * returns false.
   */
  virtual bool is_gate() const;

protected:
  Object();

  /*
   * Common implementation for refusing a bad selector.
   */
  void do_badop(Message const &, Keys &);

private:
  TableIndex _index;
};

}  // namespace k

#endif  // K_OBJECT_H
