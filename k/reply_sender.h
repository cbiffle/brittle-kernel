#ifndef K_REPLY_SENDER_H
#define K_REPLY_SENDER_H

#include "common/message.h"

#include "k/key.h"
#include "k/keys.h"
#include "k/sender.h"

namespace k {

class ReplySender final : public Sender {
public:
  /*
   * ReplySender specifics
   */

  ReplySender();
  explicit ReplySender(Message const &);

  Message & get_message() { return _m; }
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
