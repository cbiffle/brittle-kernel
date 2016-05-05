#ifndef K_TESTUTIL_SPY_H
#define K_TESTUTIL_SPY_H

/*
 * A Spy is a utility class for receiving and inspecting messages, particularly
 * reply messages.
 */

#include "common/message.h"

#include "k/keys.h"
#include "k/object.h"

namespace k {

class Sender;  // see: k/sender.h

class Spy : public Object {
public:
  Spy(Generation g, Kind k)
    : Object{g},
      _received{{}, 0},
      _count{},
     _kind{k} {}

  ReceivedMessage const & message() const { return _received; }
  Keys & keys() { return _keys; }
  unsigned count() const { return _count; }

  void set_kind(Kind k) { _kind = k; }

  Kind get_kind() const override { return _kind; }
  void deliver_from(Brand const &, Sender *) override;

private:
  ReceivedMessage _received;
  Keys _keys;
  unsigned _count;
  Kind _kind;
};

}  // namespace k

#endif  // K_TESTUTIL_SPY_H
