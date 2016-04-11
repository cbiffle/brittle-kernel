#ifndef K_SENDER_H
#define K_SENDER_H

#include "common/abi_types.h"
#include "common/exceptions.h"
#include "common/message.h"

namespace k {

struct BlockingSender;  // see: k/blocking_sender.h
struct Context;         // see: k/context.h
struct Keys;            // see: k/keys.h
template<typename> struct List;  // see: k/list.h

/*
 * Interface implemented by objects that can send messages.  Note that a
 * Sender is not necessarily an Object -- the kernel occasionally creates
 * ephemeral senders to transmit reply messages (see ReplySender).
 */
class Sender {
public:
  /*
   * Indicates that an object has accepted delivery of this sender's message.
   * The sender should return the sent message, and deposit the keys into the
   * provided location (including zeroing it if required).
   *
   * This ends the non-blocking send protocol.  If the sender needs to
   * atomically transition to receive state, it should do so here.
   */
  virtual Message on_delivery_accepted(Keys &) = 0;

  /*
   * Indicates that delivery of the sender's current message has failed.
   *
   * This ends the non-blocking send protocol.
   */
  virtual void on_delivery_failed(Exception, uint32_t = 0) = 0;

  /*
   * Asks this sender to suspend sending and block on the given list.  The
   * brand originally used to begin the send is passed in here, as a blocking
   * sender probably needs to save it somewhere.  (We want the brand to stay
   * the same even if the sender's keys are asynchronously altered.)
   *
   * If the sender agrees to block, it will save the brand and add itself to
   * the given list, thus revealing its true identity as a BlockingSender.  It
   * then expects to be resumed later by a call to BlockingSender's functions
   * on_blocked_delivery_*.
   *
   * If it is not willing to block, it does nothing, and possibly delivers an
   * exception to the controlling context.
   *
   * This ends the non-blocking send protocol.
   */
  virtual void block_in_send(Brand const &, List<BlockingSender> &) = 0;
};

}  // namespace k

#endif  // K_SENDER_H
