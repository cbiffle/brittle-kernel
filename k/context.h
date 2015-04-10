#ifndef K_CONTEXT_H
#define K_CONTEXT_H

#include "etl/armv7m/mpu.h"

#include "k/blocking_sender.h"
#include "k/config.h"
#include "k/key.h"
#include "k/list.h"
#include "k/object.h"
#include "k/region.h"
#include "k/registers.h"
#include "k/sender.h"
#include "k/types.h"

namespace k {

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
class Context : public Object, public BlockingSender {
public:
  Context();

  /*************************************************************
   * Context-specific accessors for use inside the kernel.
   */

  void set_reply_gate_index(TableIndex index) { _reply_gate_index = index; }

  StackRegisters * stack() const { return _stack; }
  void set_stack(StackRegisters * s) { _stack = s; }
  Key & key(unsigned i) { return _keys[i]; }

  void nullify_exchanged_keys(unsigned preserved = 0);

  void do_syscall();

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
   *
   * TODO: it is not clear that this pulls its weight.
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
  void complete_blocked_receive(Brand, Sender *);

  /*
   * Takes a context out of receive state due to an exception.  Analog of
   * Sender::complete_blocked_send.
   *
   * TODO: it is not clear that this pulls its weight.
   */
  void complete_blocked_receive(Exception, uint32_t = 0);


  /*************************************************************
   * Implementation of Sender.
   */

  /*
   * Overridden to return this context's priority.
   */
  Priority get_priority() const override;

  /*
   * Overridden to load the message out of saved registers.
   */
  Message get_message() override;

  /*
   * Overridden to copy keys from context key registers.
   */
  Key get_message_key(unsigned index) override;

  /*
   * Overridden to indicate success and switch the context into reply state,
   * when relevant.
   */
  void complete_send() override;

  /*
   * Overridden to record the exception and abort any remaining phase.
   */
  void complete_send(Exception, uint32_t = 0) override;

  /*
   * Overridden to support real blocking if permitted by task code.
   */
  void block_in_send(Brand brand, List<BlockingSender> &) override;


  /*************************************************************
   * Implementation of BlockingSender.
   */

  Brand get_saved_brand() const override;
  void complete_blocked_send() override;
  void complete_blocked_send(Exception, uint32_t = 0) override;


  /*************************************************************
   * Implementation of Object.
   */

  void deliver_from(Brand, Sender *) override;

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

  // Brand from the key that was used in the current send, saved for
  // use later even if the key gets modified.
  Brand _saved_brand;

  TableIndex _reply_gate_index;

  Key _memory_regions[config::n_task_regions];

  Descriptor get_descriptor() const;

  void do_ipc();
  void do_copy_key();
  void do_bad_sys();

  Key make_reply_key() const;
  void complete_receive_core(Brand, Sender *);

  // Factors of deliver_from
  void read_register(Brand, Sender *, Message const &);
  void write_register(Brand, Sender *, Message const &);

  void read_key(Brand, Sender *, Message const &);
  void write_key(Brand, Sender *, Message const &);

  void read_region(Brand, Sender *, Message const &);
  void write_region(Brand, Sender *, Message const &);
};

extern Context * current;
extern List<Context> runnable;

}  // namespace k

#endif  // K_CONTEXT_H
