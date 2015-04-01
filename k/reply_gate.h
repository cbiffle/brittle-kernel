#ifndef K_REPLY_GATE_H
#define K_REPLY_GATE_H

#include "k/object.h"
#include "k/list.h"
#include "k/types.h"

namespace k {

struct Context;  // see: k/context.h
struct Sender;  // see: k/sender.h

class ReplyGate : public Object {
public:
  ReplyGate();

  /*
   * Implementation of Object
   */

  SysResult deliver_from(Brand, Sender *) override;
  SysResult deliver_to(Brand, Context *) override;

private:
  List<Context> _receivers;
  Brand _expected_brand;
};

}  // namespace k

#endif  // K_REPLY_GATE_H
