#ifndef K_ZOO_H
#define K_ZOO_H

#include "k/address_range.h"
#include "k/context.h"

namespace k {

extern Context contexts[];
extern AddressRange init_rom;
extern AddressRange init_ram;

extern Brand const init_rom_brand;
extern Brand const init_ram_brand;

void init_zoo();

}  // namespace k

#endif  // K_ZOO_H
