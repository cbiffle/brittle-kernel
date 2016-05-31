#ifndef PEANUT_CONFIG_H
#define PEANUT_CONFIG_H

/*
 * This is intended to move out into the application.
 */

namespace config {

static constexpr unsigned
  memory_map_count = 3,
  device_map_count = 1,
  extra_slot_count = 20,
  external_interrupt_count = 40;

}  // namespace config

#endif  // PEANUT_CONFIG_H
