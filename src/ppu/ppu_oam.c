#include <stdint.h>

#include "bus.h"
#include "ppu.h"

// NES primary OAM: 256 bytes (64 sprites * 4 bytes)
static uint8_t s_oam[256];

void ppu_oam_write_byte(uint8_t index, uint8_t val)
{
    s_oam[index] = val;
}

uint8_t ppu_oam_read_byte(uint8_t index)
{
    return s_oam[index];
}

uint8_t const* ppu_oam_data(void)
{
    return s_oam;
}
