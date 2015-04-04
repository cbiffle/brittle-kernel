#ifndef DEMO_IPC_H
#define DEMO_IPC_H

#include <cstdint>

struct Message { unsigned data[4]; };
struct ReceivedMessage {
  uint64_t gate_brand;
  uint64_t sender_brand;
  Message m;
};

unsigned call(unsigned index, Message const &, ReceivedMessage *);

void move_key(unsigned to, unsigned from);

#endif  // DEMO_IPC_H
