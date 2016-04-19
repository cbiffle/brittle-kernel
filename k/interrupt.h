#ifndef K_INTERRUPT_H
#define K_INTERRUPT_H

#include <cstdint>

#include "k/blocking_sender.h"
#include "k/key.h"
#include "k/list.h"
#include "k/object.h"

namespace k {

/*
 * Base class for Interrupt-like objects.  This serves as an implementation
 * factor of Interrupt and SysTick.
 */
class InterruptBase : public Object, public BlockingSender {
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
  void deliver_from(Brand const &, Sender *) override final;

  /*
   * Implementation of Sender
   */
  Message on_delivery_accepted(Keys &) override final;
  void on_delivery_failed(Exception, uint32_t = 0) override final;
  void block_in_send(Brand const &, List<BlockingSender> &) override final;

  /*
   * Implementation of BlockingSender
   */
  Priority get_priority() const override final;
  Message on_blocked_delivery_accepted(Brand &, Keys &) override final;
  void on_blocked_delivery_failed(Exception, uint32_t = 0) override final;

protected:
  InterruptBase(Generation g, Body & body) : Object{g}, _body(body) {
    _body.sender_item.owner = this;
  }
  uint32_t get_identifier() const { return _body.identifier; }

private:
  Body & _body;

  void do_set_target(Brand const &, Message const &, Keys &);
  void do_enable(Brand const &, Message const &, Keys &);

  virtual void disable_interrupt() = 0;
  virtual void clear_pending_interrupt() = 0;
  virtual void enable_interrupt() = 0;

  /*
   * Implemented to extend the protocol.  By default, replies with
   * bad_operation.
   */
  virtual void do_extension(Selector, Brand const &, Message const &, Keys &);
};

/*
 * Controls an interrupt and generates messages when it occurs.  This is the
 * general version for external interrupts.  For the SysTick interrupt, see
 * k/sys_tick.h.
 */
class Interrupt final : public InterruptBase {
public:
  explicit Interrupt(Generation g, Body & body) : InterruptBase{g, body} {}

private:
  void disable_interrupt() override;
  void enable_interrupt() override;
  void clear_pending_interrupt() override;
};

}  // namespace k

#endif  // K_INTERRUPT_H
