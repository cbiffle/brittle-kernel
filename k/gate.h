#ifndef K_GATE_H
#define K_GATE_H

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

#include "common/abi_types.h"

#include "k/object.h"
#include "k/list.h"

namespace k {

struct Context;  // see: k/context.h
struct BlockingSender;  // see: k/blocking_sender.h

class Gate final : public Object {
public:
  struct Body {
    List<BlockingSender> senders;
    List<Context> receivers;
  };

  Gate(Generation g, Body & body) : Object{g}, _body(body) {}

  void deliver_from(Brand, Sender *) override;
  void deliver_to(Context *) override;
  bool is_gate() const override;

private:
  Body & _body;
};

}  // namespace k

#endif  // K_GATE_H
