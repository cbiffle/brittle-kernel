#ifndef K_CONTEXT_H
#define K_CONTEXT_H

#include "etl/armv7m/mpu.h"

#include "common/abi_types.h"

#include "k/blocking_sender.h"
#include "k/config.h"
#include "k/key.h"
#include "k/list.h"
#include "k/object.h"
#include "k/region.h"
#include "k/registers.h"
#include "k/sender.h"

namespace k {

struct ScopedReplySender;  // see: k/reply_sender.h

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
class Context final : public Object, public BlockingSender {
public:
  Context();

  enum class State {
    stopped = 0,
    runnable,
    sending,
    receiving,
  };

  /*************************************************************
   * Context-specific accessors for use inside the kernel.
   */

  void set_reply_gate_index(TableIndex index) { _reply_gate_index = index; }

  StackRegisters * stack() const { return _stack; }
  void set_stack(StackRegisters * s) { _stack = s; }
  Key & key(unsigned i) { return _keys[i]; }

  void nullify_exchanged_keys(unsigned preserved = 0);

  void * do_ipc(void * stack, Descriptor);
  void do_copy_key(Descriptor);

  void put_message(Brand sender_brand,
                   Message const &);

  void apply_to_mpu();

  Key & memory_region(unsigned index) { return _memory_regions[index]; }

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
  // Address of the top of the context's current stack.  When the task
  // is stopped, the machine registers are pushed onto this stack.
  StackRegisters * _stack;

  // Area for saving the context's callee-save registers.
  SavedRegisters _save;

  // Keys held by the context.
  Key _keys[config::n_task_keys];

  // List item used to link this context into typed lists of contexts.
  // These are used for receiving messages and the run queue.
  List<Context>::Item _ctx_item;

  // List item used to link this context into lists of generic senders.
  List<BlockingSender>::Item _sender_item;

  Priority _priority;
  State _state;

  // Brand from the key that was used in the current send, saved for
  // use later even if the key gets modified.
  Brand _saved_brand;

  TableIndex _reply_gate_index;

  Key _memory_regions[config::n_task_regions];

  Descriptor get_descriptor() const;
  Keys & get_message_keys();

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

  struct RegisterLocation { uint32_t * addr; bool in_context; };
  Maybe<RegisterLocation> lookup_register(unsigned);
};

}  // namespace k

#endif  // K_CONTEXT_H
