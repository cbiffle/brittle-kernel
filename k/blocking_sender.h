#ifndef K_BLOCKING_SENDER_H
#define K_BLOCKING_SENDER_H

#include "common/abi_types.h"
#include "common/exceptions.h"
#include "common/message.h"

#include "k/sender.h"

namespace k {

struct Context;  // see: k/context.h
struct Key;      // see: k/key.h
template<typename> struct List;  // see: k/list.h

/*
 * Interface implemented by Senders that can block waiting for a response.
 * Note that that says *can* block, not *will* -- it's up to the object whether
 * to block for a particular message.
 */
class BlockingSender : public Sender {
public:
  /*
   * Retrieves the brand that was stashed when this sender blocked.
   *
   * Precondition: sender is blocked.
   */
  virtual Brand get_saved_brand() const = 0;

  /*
   * Notifies the BlockingSender that the send has completed successfully.
   * This finishes the blocking phase of a send begun by block_in_send.
   *
   * If the sender is doing something call-like, this is its second chance to
   * transition to a receive state.
   */
  virtual void complete_blocked_send() = 0;

  /*
   * Cancels blocked delivery of the sender's message, e.g. in the case of
   * interruption.
   */
  virtual void complete_blocked_send(Exception, uint32_t = 0) = 0;
};

}  // namespace k

#endif  // K_BLOCKING_SENDER_H
