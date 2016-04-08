#include "etl/armv7m/exception_table.h"

#define TRAP(name) \
  void etl_armv7m_ ## name ## _handler() { \
    while (true); \
  }

TRAP(nmi)
TRAP(hard_fault)
TRAP(bus_fault)
TRAP(usage_fault)
TRAP(debug_monitor)
