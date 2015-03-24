#ifndef K_NULL_OBJECT_H
#define K_NULL_OBJECT_H

#include "k/object.h"

namespace k {

class NullObject : public Object {
public:
  SysResult send(uint32_t, Message const *) override;
};

}  // namespace k

#endif  // K_NULL_OBJECT_H
