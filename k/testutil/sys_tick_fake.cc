/*
 * Creates a fake instance of the SysTick registers in RAM, where they can be
 * inspected during test.
 */

#include "etl/armv7m/sys_tick.h"

namespace etl {
namespace armv7m {

SysTick sys_tick;

}  // namespace armv7m
}  // namespace etl
