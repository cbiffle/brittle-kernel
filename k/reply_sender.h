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

  void set_message(Message const & m) { _m = m; }
  void set_key(unsigned index, Key const &);

  /*
   * Implementation of Sender
   */

  void on_delivery_accepted(Message &, Keys &) override;
  void on_delivery_failed(Exception, uint32_t = 0) override;
  void block_in_send(Brand, List<BlockingSender> &) override;

private:
  Message _m;
  Keys _keys;
};

}  // namespace k

#endif  // K_REPLY_SENDER_H
