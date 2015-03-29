#ifndef K_REPLY_GATE_H
#define K_REPLY_GATE_H

#include "k/object.h"
#include "k/list.h"

namespace k {

struct Context;  // see: k/context.h
struct Sender;  // see: k/sender.h

class ReplyGate : public Object {
public:
  /*
   * Implementation of Object
   */

  SysResult deliver_from(uint32_t, Sender *) override;
  SysResult deliver_to(uint32_t, Context *) override;

private:
  List<Context> _receivers;
};

}  // namespace k

#endif  // K_REPLY_GATE_H
