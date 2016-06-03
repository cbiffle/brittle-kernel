#include "a/k/gate.h"

#include "etl/assert.h"
#include "common/selectors.h"
#include "a/rt/ipc.h"

namespace S = selector::gate;

namespace gate {

rt::AutoKey make_client_key(unsigned k, Brand brand) {
  Message msg {
    Descriptor::call(S::make_client_key, k),
    uint32_t(brand),
    uint32_t(brand >> 32),
  };
  auto k_out = rt::AutoKey{};
  rt::ipc2(msg, 0, rt::keymap(0, k_out, 0, 0));
  ETL_ASSERT(msg.desc.get_error() == false);
  return k_out;
}

}  // namespace gate
