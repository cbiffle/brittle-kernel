#ifndef K_CONTEXT_H
#define K_CONTEXT_H

/*
 * A hardware execution context -- the kernel side of what an application
 * might call a task.
 *
 * Contexts model only the unprivileged execution environment of the processor,
 * plus some state needed by the kernel and scheduler.
 *
 * A context key gives authority to inspect and alter this machine state, *not*
 * to communicate with the code it describes; that authority would be conferred
 * by a gate key.
 */

#include "etl/armv7m/mpu.h"

#include "common/abi_types.h"

#include "k/blocking_sender.h"
#include "k/config.h"
#include "k/key.h"
#include "k/list.h"
#include "k/maybe.h"
#include "k/object.h"
#include "k/region.h"
#include "k/registers.h"
#include "k/sender.h"

namespace k {

/*
 * Head portion of a Context object.
 */
class Context final : public Object, public BlockingSender {
public:
  enum class State {
    stopped = 0,
    runnable,
    sending,
    receiving,
  };

  struct Body {
    // Area for saving the context's callee-save registers.
    SavedRegisters save{};

    // Keys held by the context.
    Key keys[config::n_task_keys]{};

    // List item used to link this context into typed lists of contexts.
    // These are used for receiving messages and the run queue.
    List<Context>::Item ctx_item{nullptr};

    // List item used to link this context into lists of generic senders.
    List<BlockingSender>::Item sender_item{nullptr};

    Priority priority{0};
    State state{State::stopped};

    // Brand from the key that was used in the current send, saved for
    // use later even if the key gets modified.
    Brand saved_brand{0};

    Key memory_regions[config::n_task_regions]{};

    Brand expected_reply_brand{0};
  };

  Context(Generation g, Body &);

  /*************************************************************
   * Context-specific accessors for use inside the kernel.
   */

  uint32_t stack() const { return _body.save.named.stack; }
  void set_stack(uint32_t s) { _body.save.named.stack = s; }
  Key & key(unsigned i) { return _body.keys[i]; }
  Key & memory_region(unsigned index) {
    return _body.memory_regions[index];
  }

  /*
   * Checks whether this Context is waiting to receive, but is not on a block
   * list.  This condition indicates that the Context is blocked on its bound
   * Reply Gate.
   */
  bool is_awaiting_reply() const {
    return _body.state == State::receiving && !_body.ctx_item.is_linked();
  }

  uint32_t do_ipc(uint32_t stack, Descriptor);
  void do_key_op(uint32_t sysnum, Descriptor);

  void apply_to_mpu();

  /*
   * Inserts this Context onto the runnable list and pends a context switch.
   * Mostly used as an internal implementation factor of state changes, this
   * function is also used to initialize the scheduler with the first context.
   */
  void make_runnable();

  /*
   * Context-specific analog to complete_send.  Notifies the Context of a
   * receive operation that was able to complete without blocking, giving it a
   * chance to transfer the message.
   */
  void complete_receive(BlockingSender *);

  /*
   * Context-specific analog to complete_send.  This is a convenience function
   * for cancelling a receive operation without blocking.
   */
  void complete_receive(Exception, uint32_t = 0);

  /*
   * Context-specific analog to block_in_send.  Asks the Context to block
   * itself on the provided list, preparing to receive a message.
   *
   * The context can later become runnable once again through an invocation
   * of either complete_blocked_receive or interrupt.
   */
  void block_in_receive(List<Context> &);

  /*
   * Takes a context out of receive state due to reception of a message from
   * the provided sender.  Analog of Sender::complete_blocked_send.
   */
  void complete_blocked_receive(Brand const &, Sender *);

  /*
   * Takes a context out of receive state due to an exception.  Analog of
   * Sender::complete_blocked_send.
   *
   * This overload exists primarily for unblocking a blocked context.
   */
  void complete_blocked_receive(Exception, uint32_t = 0);


  /*************************************************************
   * Implementation of Sender.
   */

  /*
   * Overridden to indicate success and switch the context into reply state,
   * when relevant.
   */
  Message on_delivery(KeysRef) override;

  /*
   * Overridden to support real blocking if permitted by task code.
   */
  void block_in_send(Brand const &, List<BlockingSender> &) override;


  /*************************************************************
   * Implementation of BlockingSender.
   */

  Priority get_priority() const override;
  ReceivedMessage on_blocked_delivery(KeysRef) override;
  void on_blocked_delivery_aborted() override;


  /*************************************************************
   * Implementation of Object.
   */

  void deliver_from(Brand const &, Sender *) override;
  void deliver_to(Brand const &, Context *) override;
  Kind get_kind() const override { return Kind::context; }

private:
  Body & _body;

  Descriptor get_descriptor() const { return _body.save.sys.m.desc; }

  Key make_reply_key();
  static bool is_reply_brand(Brand const &);

  KeysRef get_receive_keys();
  KeysRef get_sent_keys();

  void handle_protocol(Brand const &, Sender *);
  void block_in_reply();

  void invalidation_hook() override;
};

}  // namespace k

#endif  // K_CONTEXT_H
