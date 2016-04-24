#ifndef K_INTERRUPT_H
#define K_INTERRUPT_H

/*
 * Controls an interrupt and generates messages when it occurs.
 */

#include <cstdint>

#include "k/blocking_sender.h"
#include "k/key.h"
#include "k/list.h"
#include "k/object.h"

namespace k {

class Interrupt final : public Object, public BlockingSender {
public:
  struct Body {
    Brand saved_brand{0};
    Key target{};
    List<BlockingSender>::Item sender_item;
    Priority priority{0};
    uint32_t identifier;

    Body(uint32_t id)
      : sender_item{nullptr},
        identifier{id} {}
  };

  Interrupt(Generation g, Body & body);

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

  Kind get_kind() const override { return Kind::interrupt; }
  void deliver_from(Brand, Sender *) override;

  /*
   * Implementation of Sender
   */
  Message on_delivery_accepted(Keys &) override;
  void block_in_send(Brand, List<BlockingSender> &) override;

  /*
   * Implementation of BlockingSender
   */
  Priority get_priority() const override;
  ReceivedMessage on_blocked_delivery_accepted(Keys &) override;
  void on_blocked_delivery_aborted() override;

private:
  Body & _body;

  uint32_t get_identifier() const { return _body.identifier; }

  void do_set_target(Brand, Message const &, Keys &);
  void do_enable(Brand, Message const &, Keys &);

  void disable_interrupt();
  void clear_pending_interrupt();
  void enable_interrupt();
};

}  // namespace k

#endif  // K_INTERRUPT_H
