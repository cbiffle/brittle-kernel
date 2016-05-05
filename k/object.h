#ifndef K_OBJECT_H
#define K_OBJECT_H

/*
 * Most kernel entities and services are represented by Objects.  Objects are
 * tracked by the ObjectTable, can receive (and possibly send) messages, and
 * are given the power to vet any keys that reference them.
 */

#include <stdint.h>

#include "common/abi_types.h"
#include "common/message.h"

#include "k/key.h"
#include "k/list.h"
#include "k/maybe.h"
#include "k/region.h"

namespace k {

struct Context;    // see: k/context.h
struct Keys;       // see: k/keys.h
struct Sender;     // see: k/sender.h

class Object {
public:
  static constexpr unsigned max_head_size = 16;

  enum class Kind : uint32_t {
    null,
    object_table,
    slot,
    memory,
    context,
    gate,
    reply_gate,
    interrupt,
  };

  /*
   * Sets the generation number for this object.
   */
  void set_generation(Generation g) { _generation = g; }

  /*
   * Gets the generation number for this object.
   */
  Generation get_generation() const { return _generation; }

  /*
   * Determines the kind (subclass) of this object.  This is used inside the
   * kernel when a reference to a particular type is required.
   *
   * Subclasses should implement this honestly.  Returning a given Kind also
   * implies permission to static_cast this Object to the corresponding
   * subclass.
   */
  virtual Kind get_kind() const = 0;

  /*
   * Generates a key to this object with the given brand, if the brand is
   * acceptable for this object.  The default implementation accepts all
   * brands.  Subclasses can implement this to be more selective.
   */
  virtual Maybe<Key> make_key(Brand const &);

  /*
   * Delivers a message from 'sender' to this object, through a key bearing
   * 'brand'.
   *
   * If this object can reason about the send right away, it should:
   * - Use sender's member functions to collect information about the message.
   * - Call the sender's on_delivery function to end the send phase.
   * - If doing a call-style invocation, generate a reply to sender's k0.
   *
   * This object may not be able to accept the message right away, in which
   * case it should ask the sender to block by calling block_in_send.  The
   * sender may or may not agree to do so, but that's between the sender and
   * the provided list.
   *
   * Should the sender agree to block, this object can inspect the sender's
   * message using its member functions while the sender is blocked.  Once this
   * object decides to unblock the sender and take action on the message, it
   * should call on_blocked_delivery in place of normal on_delivery and finish
   * the process above.
   *
   * The default implementation reports failure to the sender with a
   * bad_operation exception.
   */
  virtual void deliver_from(Brand const &, Sender *);

  /*
   * The given Context wants to receive a message from this object.  This
   * operation is typically valid only on Gates, but is implemented uniformly
   * on all objects to avoid the need for a cast.
   *
   * If this object has something to say right now, it should:
   * - Call the context's complete_receive member function, providing a
   *   BlockingSender ready to deliver.  (This makes sense mostly on Gates,
   *   where the BlockingSender will be a *different* object.)
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
   * Returns the MPU Region that should be loaded if a key to this object with
   * the given brand is found in an MPU Region Register.  The default
   * implementation for non-Memory objects returns zeroes, producing a disabled
   * region.
   */
  virtual Region get_region_for_brand(Brand const &) const;

protected:
  Object(Generation);

  /*
   * Common implementation for refusing a bad selector.
   */
  void do_badop(Message const &, Keys &);

private:
  Generation _generation;
};

template <typename T, unsigned size>
struct ObjectSubclassChecks {
  static_assert(sizeof(T) <= Object::max_head_size,
      "Object subclass head too big");

  static_assert(sizeof(typename T::Body) <= size,
      "Object subclass body too big");
};

template <typename T>
struct ObjectSubclassChecks<T, 0> {
  static_assert(sizeof(T) <= Object::max_head_size,
      "Object subclass head too big");
};

}  // namespace k

#endif  // K_OBJECT_H
