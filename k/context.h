#ifndef K_CONTEXT_H
#define K_CONTEXT_H

#include <stdint.h>

#include "etl/armv7m/mpu.h"

#include "k/config.h"
#include "k/key.h"
#include "k/list.h"
#include "k/object.h"
#include "k/registers.h"
#include "k/sender.h"

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
class Context : public Object, public Sender {
public:
  struct Region {
    etl::armv7m::Mpu::rbar_value_t rbar;
    etl::armv7m::Mpu::rasr_value_t rasr;
  };

  Context();

  /*************************************************************
   * Context-specific accessors for use inside the kernel.
   */

  void set_reply_gate_index(unsigned index) { _reply_gate_index = index; }

  Registers * stack() const { return _stack; }
  void set_stack(Registers * s) { _stack = s; }
  Key & key(unsigned i) { return _keys[i]; }

  void nullify_exchanged_keys(unsigned preserved = 0);

  SysResult do_send(bool call);

  SysResult put_message(uint32_t gate_brand,
                        uint32_t sender_brand,
                        Message const &);

  void apply_to_mpu();

  Region & region(unsigned index) { return _regions[index]; }

  /*
   * Context-specific analog to block_in_send.  Asks the Context to save the
   * (gate) brand and block itself on the provided list, preparing to receive a
   * message.
   *
   * If the context is unwilling to block, it returns SysResult::would_block.
   * Any failure will be reported back as the status of the receive.
   *
   * The context can later become runnable once again through an invocation
   * of either complete_blocked_receive or interrupt.
   */
  SysResult block_in_receive(uint32_t brand, List<Context> &);

  /*
   * Takes a context out of receive state due to reception of a message from
   * the provided sender.  Analog of Sender::complete_blocked_send.
   *
   * Unlike complete_blocked_send, this can fail, typically due to faults
   * attempting to access the delivered message.  Such failures will be
   * reported back to the sender as delivery failures.
   *
   * The brand here is the brand of key used by the sender to initiate the
   * message.
   */
  SysResult complete_blocked_receive(uint32_t brand, Sender *);


  /*************************************************************
   * Implementation of Sender.
   */

  /*
   * Overridden to return this context's priority.
   */
  uint32_t get_priority() const override;

  /*
   * Overridden to safely load this context's outgoing message from
   * unprivileged space.
   */
  SysResultWith<Message> get_message() override;

  /*
   * Overridden to copy keys from context key registers.
   */
  Key get_message_key(unsigned index) override;

  /*
   * Overridden to store the result into data registers and transition
   * the context into reply state, when relevant.
   */
  void complete_send(SysResult) override;

  /*
   * Overridden to support real blocking if permitted by task code.
   */
  SysResult block_in_send(uint32_t brand, List<Sender> &) override;

  void complete_blocked_send() override;


  /*************************************************************
   * Implementation of Object.
   */

  SysResult deliver_from(uint32_t, Sender *) override;

private:
  // Address of the top of the context's current stack.  When the task
  // is stopped, the machine registers are pushed onto this stack.
  Registers * _stack;

  // Area for saving the context's callee-save registers.
  uint32_t _registers[8];

  // Keys held by the context.
  Key _keys[config::n_task_keys];

  // List item used to link this context into typed lists of contexts.
  // These are used for receiving messages and the run queue.
  List<Context>::Item _ctx_item;

  // List item used to link this context into lists of generic senders.
  List<Sender>::Item _sender_item;

  uint32_t _priority;

  // Brand from the key that was used in the current send, saved for
  // use later even if the key gets modified.
  uint32_t _saved_brand;
  bool _calling;

  uint32_t _reply_gate_index;

  Region _regions[config::n_task_regions];

  // Factors of deliver_from
  SysResult read_register(uint32_t, Sender *, Message const &);
  SysResult write_register(uint32_t, Sender *, Message const &);

  SysResult read_key(uint32_t, Sender *, Message const &);
  SysResult write_key(uint32_t, Sender *, Message const &);

  SysResult read_region(uint32_t, Sender *, Message const &);
  SysResult write_region(uint32_t, Sender *, Message const &);
};

extern Context * current;
extern List<Context> runnable;

}  // namespace k

#endif  // K_CONTEXT_H
