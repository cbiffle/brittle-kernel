#ifndef K_REPLY_GATE_H
#define K_REPLY_GATE_H

/*
 * A specialized kind of message gate used to receive a *single* reply message
 * and then atomically revoke all keys to the gate.
 *
 * Each Context possesses a ReplyGate.  When unprivileged code triggers a
 * call-style IPC, the delivered message contains a fresh key to the ReplyGate.
 *
 * Each ReplyGate contains a 96-bit counter (32 bits of which are in the Object
 * Table generation field).  Whenever a ReplyGate key is used, the ReplyGate
 * checks the key's embedded brand and generation number for a match; on
 * mismatch, for example because a stale reply key was kept around, the key
 * behaves as null.
 *
 * If the key contents match the counter, on the other hand, the key works --
 * once.  The ReplyGate's internal counter is advanced, implicitly revoking the
 * key for future operations.
 *
 * TODO: currently, the ReplyGate's counter is not revved if the Context is
 * broken out of receive.
 */

#include "common/abi_types.h"

#include "k/key.h"
#include "k/list.h"
#include "k/object.h"

namespace k {

struct Context;  // see: k/context.h
struct Sender;  // see: k/sender.h

class ReplyGate final : public Object {
public:
  struct Body {
    List<Context> receivers{};
    Brand expected_brand{0};
  };

  ReplyGate(Generation g, Body & body) : Object{g}, _body(body) {}

  /*
   * Implementation of Object
   */

  void deliver_from(Brand const &, Sender *) override;
  void deliver_to(Context *) override;
  Maybe<Key> make_key(Brand) override;

private:
  Body & _body;
};

}  // namespace k

#endif  // K_REPLY_GATE_H
