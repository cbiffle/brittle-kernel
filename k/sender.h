#ifndef K_SENDER_H
#define K_SENDER_H

#include "k/exceptions.h"
#include "k/sys_result.h"
#include "k/types.h"

namespace k {

struct Context;  // see: k/context.h
struct Key;      // see: k/key.h
struct Message;  // see: k/ipc.h
template<typename> struct List;  // see: k/list.h

/*
 * Interface implemented by objects that can send messages.  Note that a
 * Sender is not necessarily an Object -- the kernel occasionally creates
 * ephemeral senders to transmit reply messages.
 */
class Sender {
public:
  /*
   * Returns the priority associated with this sender's current message.
   * This is used to determine the order in which pending messages are
   * processed.
   */
  virtual Priority get_priority() const = 0;

  /*
   * Returns a copy of the sender's current message.
   */
  virtual Message get_message() = 0;

  /*
   * Copies one of the message keys.
   *
   * Precondition: the index is less than config::n_message_keys.
   *
   * The default implementation delivers null keys.
   */
  virtual Key get_message_key(unsigned index);

  /*
   * Finishes a send without blocking, indicating success to the sender.
   *
   * This is one of two opportunities for the sender to atomically
   * transition to receive state, if desired; the other is the
   * similar function complete_blocked_send.
   */
  virtual void complete_send() = 0;

  /*
   * Finishes a send without blocking, indicating failure to the sender.  This
   * means the message was not delivered, not that the message's contents were
   * rejected.
   */
  virtual void complete_send(Exception, uint32_t = 0) = 0;

  /*
   * Asks this sender to suspend sending and block on the given list.  The
   * brand originally used to begin the send is passed in here, as a blocking
   * sender probably needs to save it somewhere.  (We want the brand to stay
   * the same even if the sender's keys are asynchronously altered.)
   *
   * (TODO this may be excessively paranoid.)
   *
   * If the sender agrees to block, it will save the brand and add itself to
   * the given list, expecting to be freed later either by interruption or a
   * call to complete_blocked_send.
   *
   * If it is not willing to block, it does nothing, and may record somewhere
   * that the send was not completed.
   */
  virtual void block_in_send(Brand, List<Sender> &) = 0;

  /*
   * Finishes the blocking phase of a send begun by block_in_send, indicating
   * that delivery was successful.
   *
   * If the sender is doing something call-like, this is its second chance to
   * transition to a receive state.
   *
   * Senders must implement this if they ever add themselves to a sender list
   * as in block_in send.  The default implementation asserts!
   */
  virtual void complete_blocked_send();

  /*
   * Finishes the blocking phase of a send begun by block_in_send, recording
   * the given delivery error.
   *
   * Senders must implement this if they ever add themselves to a sender list
   * as in block_in send.  The default implementation asserts!
   */
  virtual void complete_blocked_send(Exception, uint32_t = 0);
};

}  // namespace k

#endif  // K_SENDER_H
