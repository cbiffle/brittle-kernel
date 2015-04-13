#ifndef DEMO_RUNTIME_IPC_H
#define DEMO_RUNTIME_IPC_H

#include <cstdint>

#include "common/message.h"

ReceivedMessage ipc(Message const &);

void copy_key(unsigned to, unsigned from);

#endif  // DEMO_RUNTIME_IPC_H
