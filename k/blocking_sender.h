#ifndef K_BLOCKING_SENDER_H
#define K_BLOCKING_SENDER_H

#include "etl/data/range_ptr.h"

#include "common/abi_types.h"
#include "common/exceptions.h"
#include "common/message.h"

#include "k/sender.h"

namespace k {

struct Context;  // see: k/context.h
struct Keys;     // see: k/keys.h
template<typename> struct List;  // see: k/list.h

/*
 * Interface implemented by Senders that can block waiting for a response.
 * Notice that they *can* block, not *will* -- it's up to the object whether
 * to block for a particular message.  For instance, a Context may or may not
 * block depending on the IPC descriptor specified by its unprivileged code.
 */
class BlockingSender : public Sender {
public:
  /*
   * Returns the priority associated with this sender's current message.  This
   * is used to determine the order in which pending messages are processed,
   * and to maintain the order of block queues.
   */
  virtual Priority get_priority() const = 0;

  /*
   * Indicates that an object has accepted delivery of the blocked message.
   * Analog to Sender::on_delivery_accepted.
   *
   * This ends the blocking send protocol.  If the sender needs to atomically
   * transition to receive state, it should do so here.
   */
  virtual void on_blocked_delivery_accepted(Message &, Brand &, Keys &) = 0;

  /*
   * Indicates that delivery of the blocked message has failed.
   * Analog to Sender::on_delivery_failed.
   *
   * This can also be used to interrupt a pending sender.
   *
   * This ends the blocking send protocol.
   */
  virtual void on_blocked_delivery_failed(Exception, uint32_t = 0) = 0;
};

}  // namespace k

#endif  // K_BLOCKING_SENDER_H
