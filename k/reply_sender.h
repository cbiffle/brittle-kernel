#ifndef K_REPLY_SENDER_H
#define K_REPLY_SENDER_H

/*
 * Defines ReplySender and ScopedReplySender, utility classes for sending
 * messages from kernel objects back to the application.
 *
 * Kernel objects agree to send a single, non-blocking reply in response to any
 * operation.  It may indicate success or failure.  This implies the existence,
 * at least temporarily, of a k::Sender object to participate in the message
 * delivery virtual protocol.  While some kernel objects are, themselves,
 * k::Senders, they may be busy sending some *other* message.
 *
 * Thus a dedicated Sender is needed.
 *
 * But since the reply from the kernel cannot block, the Sender doesn't need to
 * be a BlockingSender, so it can exist transitively on the stack.  This is
 * good, as the alternatives -- allocating one spare Sender per kernel object,
 * for example -- would waste RAM.
 *
 * ReplySender is a Sender that's designed to be stack allocated to deliver a
 * single message.
 *
 * ScopedReplySender is a wrapper that makes ReplySender easier to use.
 *
 * TODO: the two are separate for historical reasons.  It might make sense to
 * fold them together someday, since I'm not using ReplySender without Scoped
 * in practice.
 */

#include "common/message.h"

#include "k/key.h"
#include "k/keys.h"
#include "k/sender.h"

namespace k {

/*
 * Acts as a Sender for a configurable message and block of keys, refusing to
 * block.
 */
class ReplySender final : public Sender {
public:
  /*
   * ReplySender specifics
   */

  // Constructs a ReplySender with a default-constructed (zero) message and
  // null keys.
  ReplySender();

  // Constructs a ReplySender with the given message and null keys.
  explicit ReplySender(Message const &);

  // Gets a mutable reference to the contained message, so it can be updated.
  Message & get_message() { return _m; }

  // Replaces one of the keys in the contained message.
  void set_key(unsigned index, Key const &);

  /*
   * Implementation of Sender
   */

  void on_delivery_accepted(Message &, Keys &) override;
  void on_delivery_failed(Exception, uint32_t = 0) override;
  void block_in_send(Brand const &, List<BlockingSender> &) override;

private:
  Message _m;
  Keys _keys;
};

/*
 * Variant on ReplySender that automatically delivers a reply when it goes out
 * of scope.
 *
 * TODO: fold the two together?
 */
class ScopedReplySender final {
public:
  ScopedReplySender(Key & key) : rs{}, k(key) {}
  explicit ScopedReplySender(Key & key, Message const &m) : rs{m}, k(key) {}

  ~ScopedReplySender() {
    k.deliver_from(&rs);
  }

  Message & get_message() { return rs.get_message(); }
  void set_key(unsigned index, Key const & k) { rs.set_key(index, k); }

  ReplySender rs;
  Key & k;
};

}  // namespace k

#endif  // K_REPLY_SENDER_H
