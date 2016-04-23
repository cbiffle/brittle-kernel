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

struct ReplyGate;  // see: k/reply_gate.h
struct ScopedReplySender;  // see: k/reply_sender.h

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

    // Address of the top of the context's current stack.  When the task
    // is stopped, the machine registers are pushed onto this stack.
    StackRegisters * stack{nullptr};

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

    Maybe<Object *> reply_gate{nothing};

    Key memory_regions[config::n_task_regions]{};
  };

  Context(Generation g, Body &);

  /*************************************************************
   * Context-specific accessors for use inside the kernel.
   */

  StackRegisters * stack() const { return _body.stack; }
  void set_stack(StackRegisters * s) { _body.stack = s; }
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

  void set_reply_gate(ReplyGate &);

  void nullify_exchanged_keys(unsigned preserved = 0);

  void * do_ipc(void * stack, Descriptor);
  void do_copy_key(Descriptor);

  void apply_to_mpu();

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
   * Analog of block_in_receive for particular use by ReplyGate.
   */
  void block_in_reply();

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

  /*
   * Switches a Context to runnable state, aborting any IPC in progress.
   */
  void make_runnable();


  /*************************************************************
   * Implementation of Sender.
   */

  /*
   * Overridden to indicate success and switch the context into reply state,
   * when relevant.
   */
  Message on_delivery_accepted(Keys &) override;

  /*
   * Overridden to record the exception and abort any remaining phase.
   */
  void on_delivery_failed(Exception, uint32_t = 0) override;

  /*
   * Overridden to support real blocking if permitted by task code.
   */
  void block_in_send(Brand const &, List<BlockingSender> &) override;


  /*************************************************************
   * Implementation of BlockingSender.
   */

  Priority get_priority() const override;
  Message on_blocked_delivery_accepted(Brand &, Keys &) override;
  void on_blocked_delivery_failed(Exception, uint32_t = 0) override;


  /*************************************************************
   * Implementation of Object.
   */

  void deliver_from(Brand const &, Sender *) override;

private:
  Body & _body;

  Descriptor get_descriptor() const { return _body.save.sys.m.d0; }
  Keys & get_message_keys() { return *reinterpret_cast<Keys *>(_body.keys); }

  Key make_reply_key() const;

  // Factors of deliver_from
  void do_read_register(ScopedReplySender &, Brand const &, Message const &, Keys &);
  void do_write_register(ScopedReplySender &, Brand const &, Message const &, Keys &);

  void do_read_key(ScopedReplySender &, Brand const &, Message const &, Keys &);
  void do_write_key(ScopedReplySender &, Brand const &, Message const &, Keys &);

  void do_read_region(ScopedReplySender &, Brand const &, Message const &, Keys &);
  void do_write_region(ScopedReplySender &, Brand const &, Message const &, Keys &);

  void do_make_runnable(ScopedReplySender &, Brand const &, Message const &, Keys &);

  void do_read_priority(ScopedReplySender &, Brand const &, Message const &, Keys &);
  void do_write_priority(ScopedReplySender &, Brand const &, Message const &, Keys &);

  void do_save_kernel_registers(ScopedReplySender &, Brand const &,
      Message const &, Keys &);
  void do_restore_kernel_registers(ScopedReplySender &, Brand const &,
      Message const &, Keys &);

  Maybe<uint32_t *> lookup_register(unsigned);
};

}  // namespace k

#endif  // K_CONTEXT_H
