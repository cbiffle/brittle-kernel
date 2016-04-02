#ifndef K_GATE_H
#define K_GATE_H

#include "common/abi_types.h"

#include "k/object.h"
#include "k/list.h"

namespace k {

struct Context;  // see: k/context.h
struct BlockingSender;  // see: k/blocking_sender.h

/*
 * A Gate is an Object that can emulate other objects by collaborating with
 * code running in one or more Contexts.
 *
 * Gates are transparent: they have no native protocol.  Any attempt to send to
 * a Gate will actually send *through* the gate:
 * - If code is waiting to receive on the same gate, the message is routed
 *   to it.
 * - Otherwise, the sender is blocked until a receive happens.
 */
class Gate final : public Object {
public:
  void deliver_from(Brand const &, Sender *) override;
  void deliver_to(Context *) override;
  bool is_gate() const override;

private:
  List<BlockingSender> _senders;
  List<Context> _receivers;
};

}  // namespace k

#endif  // K_GATE_H
