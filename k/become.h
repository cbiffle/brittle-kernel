#ifndef K_BECOME_H
#define K_BECOME_H

/*
 * Operations for converting kernel objects between types.
 */

#include "common/message.h"

namespace k {

struct Memory;  // see: k/memory.h
struct Keys;  // see: k/keys.h
struct ReplySender;  // see: k/reply_sender.h

void become(Memory &, Message const &, Keys &, ReplySender &);

}  // namespace k

#endif  // K_BECOME_H
