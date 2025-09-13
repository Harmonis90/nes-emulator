//
// Created by Seth on 9/10/2025.
//

#pragma once

#include <stdint.h>

typedef enum
{
    MIRROR_HORIZONTAL,
    MIRROR_VERTICAL,
    MIRROR_SINGLE_LO,
    MIRROR_SINGLE_HI,
    MIRROR_FOUR
} mirroring_t;

// Provided by cartridge/mapper (or a forwarder in ppu_regs)
mirroring_t ppu_mem_get_mirroring(void);

void ppu_mem_reset(void);

// Raw PPU address space (PPU side): 0x0000-0x3FFF
uint8_t ppu_mem_read(uint16_t addr);
void ppu_mem_write(uint16_t addr, uint8_t val);