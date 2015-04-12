#ifndef DEMO_CONTEXT_H
#define DEMO_CONTEXT_H

#include <cstdint>

namespace context {

bool set_region(unsigned k, unsigned region_index, unsigned region_key);
bool get_region(unsigned k, unsigned region_index, unsigned region_key_out);

bool set_register(unsigned k, unsigned index, uint32_t value);

bool set_key(unsigned k, unsigned key_index, unsigned key);

bool make_runnable(unsigned k);

}  // namespace context

#endif  // DEMO_CONTEXT_H
