#ifndef K_CONFIG_H
#define K_CONFIG_H

namespace k {
namespace config {

static constexpr unsigned
  n_contexts = 2,
  n_objects = 10,
  n_task_keys = 16,
  n_task_regions = 6,
  n_message_data = 4,
  n_message_keys = 4,
  n_priorities = 4;

}  // namespace config
}  // namespace k

#endif // K_CONFIG_H
