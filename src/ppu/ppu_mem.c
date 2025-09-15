#include <stdint.h>
#include <string.h>

#include "ppu_mem.h"
#include "mapper.h"


// 2KB nametable VRAM: NT0 ($2000-$23FF) + NT1 ($2400-$27FF)
// NT2/NT3 mirror onto these two based on mirroring mode
static uint8_t s_vram[0x800];
static uint8_t s_palette[0x20];
static mirroring_t s_mirr = MIRROR_HORIZONTAL;

static inline uint16_t mirror_nt_addr(uint16_t addr)
{
    // Normalize to $2000-$2FFF
    uint16_t v = (uint16_t)((addr - 0x2000) & 0x0FFF);
    uint16_t nt = v >> 10; // 0..3 (which nametable)
    uint16_t off = (v & 0x03FF);

    switch (s_mirr)
    {
        case MIRROR_HORIZONTAL:
            // [A A B B]  -> NT0 for 0,1 ; NT1 for 2,3
            return (uint16_t)((nt <= 1 ? 0x000 : 0x400) + off);

    case MIRROR_VERTICAL:
            // [A B A B]  -> NT0 for 0,2 ; NT1 for 1,3
            return (uint16_t)(((nt & 1) ? 0x400 : 0x00) + off);

        case MIRROR_SINGLE_LO:
            return off;

        case MIRROR_SINGLE_HI:
            return (uint16_t)(0x400 + off);

    case MIRROR_FOUR:
            return (uint16_t)(((nt & 1) ? 0x400: 0x000) + off);
    }
    return off;
}

static inline uint16_t mirror_palette_addr(uint16_t addr)
{
    // Palettes mirror every 32 bytes
    uint16_t a = (uint16_t)(0x3F00 + ((addr - 0x3F00) & 0x1F));

    // Hardware alias: $3F10/$14/$18/$1C mirror $3F00/$04/$08/$0C
    if ((a & 0x0010) && ((a & 0x003) == 0))
    {
        a = (uint16_t)(a - 0x0010);
    }
    return a;
}

void ppu_mem_set_mirroring(mirroring_t m)
{
    s_mirr = m;
}

void ppu_mem_init(mirroring_t m)
{
    s_mirr = m;
    memset(s_vram, 0, sizeof(s_vram));
    memset(s_palette, 0, sizeof(s_palette));
}

uint8_t ppu_mem_read(uint16_t addr)
{
    addr &= 0x3FFF;
    if (addr < 0x2000)
    {
        return mapper_chr_read(addr);
    }
    else if (addr < 0x3F00)
    {
        uint16_t vr = mirror_nt_addr(addr);
        return s_vram[vr & 0x7FF];
    }
    else
    {
        uint16_t a = mirror_palette_addr(addr);
        return s_palette[a & 0x1F];
    }
}

void ppu_mem_write(uint16_t addr, uint8_t data)
{
    addr &= 0x3FFF;

    if (addr < 0x2000)
    {
        mapper_chr_write(addr, data);
    }
    else if (addr < 0x3F00)
    {
        uint16_t vr = mirror_nt_addr(addr);
        s_vram[vr & 0x7FF] = data;
    }
    else
    {
        uint16_t a = mirror_palette_addr(addr);
        s_palette[a & 0x1F] = data;
    }
}