#ifndef DEMO_DRV_UART_API_H
#define DEMO_DRV_UART_API_H

#include <cstdint>

namespace demo {
namespace drv {
namespace uart {

bool send(unsigned k, uint8_t);

}  // namespace uart
}  // namespace drv
}  // namespace demo

#endif  // DEMO_DRV_UART_API_H
