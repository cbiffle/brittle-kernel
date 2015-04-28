#ifndef K_INTERRUPT_H
#define K_INTERRUPT_H

#include "k/blocking_sender.h"
#include "k/list.h"
#include "k/object.h"

namespace k {

class Interrupt final : public Object, public BlockingSender {
public:
  explicit Interrupt(unsigned irq);

  /*
   * Triggers this interrupt.  Should be called from an ISR.
   */
  void trigger();

  /*
   * Implementation of Object.
   *
   * Note that an Interrupt cannot be directly received from; its messages
   * must arrive through a gate.
   */

  /*
   * Handles a message in the interrupt control protocol.
   */
  void deliver_from(Brand, Sender *) override;

  /*
   * Implementation of Sender
   */
  Priority get_priority() const override;
  Message get_message() override;
  Key get_message_key(unsigned) override;

  void complete_send() override;
  void complete_send(Exception, uint32_t = 0) override;

  void block_in_send(Brand, List<BlockingSender> &) override;

  /*
   * Implementation of BlockingSender
   */
  Brand get_saved_brand() const override;
  void complete_blocked_send() override;
  void complete_blocked_send(Exception, uint32_t = 0) override;

private:
  Brand _saved_brand;
  Key _target;
  List<BlockingSender>::Item _sender_item;
  Priority _priority;
  unsigned _irq;

  void do_set_target(Brand, Sender *, Message const &);
  void do_enable(Brand, Sender *, Message const &);
};

}  // namespace k

#endif  // K_INTERRUPT_H
