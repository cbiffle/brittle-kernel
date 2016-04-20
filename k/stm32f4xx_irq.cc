#include "k/irq_entry.h"

#include "etl/attribute_macros.h"

namespace k {

typedef void (*InterruptHandler)(void);

struct InterruptTable {
  #define ETL_STM32F4XX_IRQ(name) InterruptHandler name ## _handler;
  #include "etl/stm32f4xx/interrupts.def"
  #undef ETL_STM32F4XX_IRQ
};

ETL_SECTION(".stm32f4xx_interrupt_redirect")
ETL_USED
InterruptTable const stm32f4xx_interrupt_redirect {
  #define ETL_STM32F4XX_IRQ(name) irq_redirector,
  #include "etl/stm32f4xx/interrupts.def"
  #undef ETL_STM32F4XX_IRQ
};

}  // namespace k
