#include "k/zoo.h"

#include "etl/stm32f4xx/interrupts.h"
#include "etl/stm32f4xx/interrupt_table.h"

#include "k/gate.h"
#include "k/interrupt.h"
#include "k/null_object.h"
#include "k/object_table.h"
#include "k/reply_gate.h"

using etl::armv7m::Mpu;

extern "C" {
  extern uint8_t _app_rom_start, _app_rom_end;
  extern uint8_t _app_ram_start, _app_ram_end;
}

namespace k {

Context contexts[config::n_contexts];
ReplyGate reply_gates[config::n_contexts];

static NullObject null;

ObjectTable object_table;

AddressRange init_rom{
  {&_app_rom_start, &_app_rom_end},
  false,
  AddressRange::ReadOnly::priv,
};

static Region init_rom_region {
  .rbar = Region::Rbar()
    .with_addr_27(reinterpret_cast<uintptr_t>(&_app_rom_start) >> 5),
  .rasr = Region::Rasr()
    .with_ap(Mpu::AccessPermissions::p_read_u_read)
    .with_size(13)
    .with_enable(true),
};
Brand const init_rom_brand = uint32_t(init_rom_region.rbar)
                           | uint64_t(uint32_t(init_rom_region.rasr)) << 32;

AddressRange init_ram{
  {&_app_ram_start, &_app_ram_end},
  false,
  AddressRange::ReadOnly::no,
};

static Region init_ram_region {
  .rbar = Region::Rbar()
    .with_addr_27(reinterpret_cast<uintptr_t>(&_app_ram_start) >> 5),
  .rasr = Region::Rasr()
    .with_ap(Mpu::AccessPermissions::p_write_u_write)
    .with_size(15)
    .with_enable(true),
};
Brand const init_ram_brand = uint32_t(init_ram_region.rbar)
                           | uint64_t(uint32_t(init_ram_region.rasr)) << 32;

AddressRange peripherals{
  {reinterpret_cast<uint8_t *>(0x40000000),
   reinterpret_cast<uint8_t *>(0x60000000)},
  true,
  AddressRange::ReadOnly::no,
};

Gate client_gate;
Gate irq_gate;

Interrupt irq{unsigned(etl::stm32f4xx::Interrupt::usart2)};

void init_zoo() {
  // First, wire up the special objects.
  auto constexpr special_objects = 8;
  object_table[0].ptr = &null;
  null.set_index(0);

  object_table[1].ptr = &object_table;
  object_table.set_index(1);

  object_table[2].ptr = &init_rom;
  init_rom.set_index(2);

  object_table[3].ptr = &init_ram;
  init_ram.set_index(3);

  object_table[4].ptr = &peripherals;
  peripherals.set_index(4);

  object_table[5].ptr = &client_gate;
  client_gate.set_index(5);

  object_table[6].ptr = &irq_gate;
  irq_gate.set_index(6);

  object_table[7].ptr = &irq;
  irq.set_index(7);

  // Then, wire up the contexts.
  static_assert(config::n_objects >= 2*config::n_contexts + special_objects,
                "Not enough object table entries for objects in zoo.");
  for (unsigned i = 0; i < config::n_contexts; ++i) {
    unsigned ctx_i = i + special_objects;
    object_table[ctx_i].ptr = &contexts[i];
    contexts[i].set_index(ctx_i);

    unsigned rep_i = ctx_i + config::n_contexts;
    object_table[rep_i].ptr = &reply_gates[i];
    reply_gates[i].set_index(rep_i);
    contexts[i].set_reply_gate_index(rep_i);
  }
}

}  // namespace k

void etl_stm32f4xx_usart2_handler() {
  k::irq.trigger();
}
