#pragma once
#include <stdint.h>

// Cartridge-controlled nametable mirroring
typedef enum
{
    MIRROR_HORIZONTAL = 0, // [A A B B]
    MIRROR_VERTICAL = 1, // [A B A B]
    MIRROR_SINGLE_LO = 2,  // all -> NT0 ($2000)
    MIRROR_SINGLE_HI = 3, // all -> NT1 ($2400)
    MIRROR_FOUR = 4 // true 4-screen (cart VRAM); we’ll fall back to V
}mirroring_t;

// Initialize/clear backing storage and set mirroring.
void ppu_mem_init(mirroring_t m);

// Change mirroring mode at runtime (mapper-controlled).
void ppu_mem_set_mirroring(mirroring_t m);
mirroring_t ppu_mem_get_mirroring(void);

// Raw PPU memory space ($0000-$3FFF) — used by PPU registers ($2007).
//  - $0000-$1FFF : CHR (ROM/RAM) via mapper
//  - $2000-$3EFF : Nametables (2KB VRAM, mirrored per 'm')
//  - $3F00-$3FFF : Palettes (mirrors every 32; with $3F10 alias fixups)
uint8_t ppu_mem_read(uint16_t addr);
void ppu_mem_write(uint16_t addr, uint8_t data);