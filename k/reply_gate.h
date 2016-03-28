#ifndef K_REPLY_GATE_H
#define K_REPLY_GATE_H

#include "common/abi_types.h"

#include "k/key.h"
#include "k/list.h"
#include "k/object.h"

namespace k {

struct Context;  // see: k/context.h
struct Sender;  // see: k/sender.h

class ReplyGate final : public Object {
public:
  ReplyGate();

  /*
   * Implementation of Object
   */

  void deliver_from(Brand const &, Sender *) override;
  void deliver_to(Context *) override;
  Maybe<Key> make_key(Brand) override;

private:
  List<Context> _receivers;
  Brand _expected_brand;
};

}  // namespace k

#endif  // K_REPLY_GATE_H
