//
// Created by Seth on 9/10/2025.
//

#include <stdint.h>
#include <string.h>
#include "mapper.h" // mapper_chr_read/mapper_chr_write (CHR ROM/RAM via cartridge)
#include "ines.h" // for mirroring enum
#include "ppu.h" // for ppu_get_mirroring() or similar
#include "ppu_mem.h"


static uint8_t vram_nt[0x800];
static uint8_t pal[0x20];

static inline uint16_t nt_index(uint16_t addr)
{
    addr = 0x2000 + (addr & 0x0FFF);
    uint16_t nt = (addr - 0x2000) >> 10;
    uint16_t off = addr & 0x03FF;

    mirroring_t m = ppu_mem_get_mirroring();
    uint16_t base;

    switch (m)
    {
        case MIRROR_HORIZONTAL: base = (nt & 0x02) ? 0x400 : 0x000; break;  // 0,1->A ; 2,3->B
        case MIRROR_VERTICAL: base = (nt & 0x01) ? 0x400 : 0x000; break;
        case MIRROR_SINGLE_LO: base = 0x00; break;
        case MIRROR_SINGLE_HI: base = 0x400; break;
        case MIRROR_FOUR: base = (nt & 0x01) ? 0x400 : 0x000; break;
        default: base = 0x000; break;
    }
    return (uint16_t)(base + off);
}

static inline uint16_t pal_index(uint16_t addr)
{
    addr &= 0x1F;
    if ((addr & 0x13) == 0x10) addr &= ~0x10;
    return addr;
}

void ppu_mem_reset(void)
{
    memset(vram_nt, 0, sizeof(vram_nt));
    memset(pal, 0, sizeof(pal));
}

uint8_t ppu_mem_read(uint16_t addr)
{
    addr &= 0x3FFF;
    if (addr < 0x2000)
    {
        return;
    }
}