#ifndef K_SENDER_H
#define K_SENDER_H

#include "k/sys_result.h"

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
  virtual uint32_t get_priority() const = 0;

  /*
   * Returns a copy of the sender's current message, or one of the
   * following error results:
   *
   * - fault: the area alleged to contain a message by unprivileged
   *   code turns out not to be accessible to said code.
   *
   * Any failure returned from this function represents a failure
   * in delivery.
   */
  virtual SysResultWith<Message> get_message() = 0;

  /*
   * Copies one of the message keys.
   *
   * Precondition: the index is less than config::n_message_keys.
   *
   * The default implementation delivers null keys.
   */
  virtual Key get_message_key(unsigned index);

  /*
   * Finishes a send without blocking, reporting the given delivery
   * result.  In general, kernel functions can return a SysResult
   * during delivery and not call this function directly -- wrappers
   * at the outermost level of the stack will call it if required.
   *
   * This is one of two opportunities for the sender to atomically
   * transition to receive state, if desired; the other is the
   * similar function complete_blocked_send.
   *
   * The default implementation does nothing.
   */
  virtual void complete_send(SysResult = SysResult::success);

  /*
   * Asks this sender to suspend sending and block on the given list.
   * The brand originally used to begin the send is passed in here, as
   * a blocking sender probably needs to save it somewhere.
   *
   * Acceptable results:
   *
   * - success: the sender has stored the brand and any additional
   *   information needed for its eventual reply, and has listed itself
   *   on the given sender list.
   *
   * - would_block: the sender refuses to block for message delivery
   *   and thus delivery should fail.
   *
   * The default implementation -- the common one for kernel objects --
   * refuses to block by returning SysResult::would_block.
   *
   * If blocking is accepted, the sender will be freed later by a call
   * to complete_blocked_send.
   */
  virtual SysResult block_in_send(uint32_t brand, List<Sender> &);

  /*
   * Finishes the blocking phase of a send begun by block_in_send.
   * Note that at this point the delivery has succeeded, so -- unlike
   * with complete_send -- there's no opportunity to indicate an error.
   *
   * If the sender is doing something call-like, this is its chance
   * to transition to a receive state.
   *
   * Any error returned here will be exposed to the *recipient* as a
   * failure in receive.
   *
   * Senders must implement this if they permit blocking in block_in_send.
   * The default implementation asserts!
   */
  virtual void complete_blocked_send();
};

}  // namespace k

#endif  // K_SENDER_H
