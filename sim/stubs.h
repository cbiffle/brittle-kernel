#ifndef SIM_STUBS_H
#define SIM_STUBS_H

#include "common.h"

SysResult send(bool block, uintptr_t t, Message const &);
SysResult call(bool block, uintptr_t t, Message const &, ReceivedMessage *);
SysResult open_receive(bool block, ReceivedMessage *);

#endif  // SIM_STUBS_H
