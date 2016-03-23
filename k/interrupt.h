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
  void deliver_from(Brand const &, Sender *) override;

  /*
   * Implementation of Sender
   */
  void on_delivery_accepted(Message &, Keys &) override;
  void on_delivery_failed(Exception, uint32_t = 0) override;
  void block_in_send(Brand const &, List<BlockingSender> &) override;

  /*
   * Implementation of BlockingSender
   */
  Priority get_priority() const override;
  void on_blocked_delivery_accepted(Message &, Brand &, Keys &) override;
  void on_blocked_delivery_failed(Exception, uint32_t = 0) override;

private:
  Brand _saved_brand;
  Key _target;
  List<BlockingSender>::Item _sender_item;
  Priority _priority;
  unsigned _irq;

  void do_set_target(Brand const &, Message const &, Keys &);
  void do_enable(Brand const &, Message const &, Keys &);
};

}  // namespace k

#endif  // K_INTERRUPT_H
