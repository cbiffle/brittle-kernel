#include "demo/runtime/ipc.h"

static_assert(sizeof(Message) == 6 * sizeof(uint32_t),
    "This code expects a six-word Message.");

static_assert(sizeof(ReceivedMessage) == 8 * sizeof(uint32_t),
    "This code expects an eight-word ReceivedMessage.");
