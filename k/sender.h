#ifndef K_SENDER_H
#define K_SENDER_H

#include "k/list.h"
#include "k/object.h"
#include "k/sys_result.h"

namespace k {

struct Message;  // see: k/ipc.h

class Sender : public Object {
public:
  uint32_t get_priority() const { return _priority; }

  virtual SysResultWith<Message> get_message() = 0;

protected:
  Sender();

private:
  List<Sender>::Item _item;
  uint32_t _priority;
};

}  // namespace k

#endif  // K_SENDER_H
