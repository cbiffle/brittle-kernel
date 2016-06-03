#ifndef A_K_GATE_H
#define A_K_GATE_H

#include <cstdint>

#include "common/abi_types.h"
#include "a/rt/keys.h"

namespace gate {

rt::AutoKey make_client_key(unsigned k, Brand);

}  // namespace gate

#endif  // A_K_GATE_H
