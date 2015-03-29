#ifndef K_OBJECT_H
#define K_OBJECT_H

#include <stdint.h>

#include "k/sys_result.h"

namespace k {

struct Message;  // see: k/ipc.h
struct Sender;  // see: k/sender.h

class Object {
public:
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
  virtual SysResult deliver_from(uint32_t brand, Sender *) = 0;
};

}  // namespace k

#endif  // K_OBJECT_H
