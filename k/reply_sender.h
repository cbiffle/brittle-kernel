#ifndef K_REPLY_SENDER_H
#define K_REPLY_SENDER_H

#include "k/ipc.h"
#include "k/key.h"
#include "k/sender.h"

namespace k {

class ReplySender : public Sender {
public:
  /*
   * ReplySender specifics
   */

  explicit ReplySender(uint32_t priority);
  explicit ReplySender(uint32_t priority, Message const &);

  void set_message(Message const & m) { _m = m; }
  void set_key(unsigned index, Key const &);

  /*
   * Implementation of Sender
   */

  uint32_t get_priority() const override;
  Message get_message() override;
  Key get_message_key(unsigned index) override;

  void complete_send() override;
  void complete_send(Exception, uint32_t) override;
  void block_in_send(Brand, List<Sender> &) override;

private:
  Message _m;
  Key _keys[config::n_message_keys];
  uint32_t _priority;
};

}  // namespace k

#endif  // K_REPLY_SENDER_H
