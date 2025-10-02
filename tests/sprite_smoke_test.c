// tests/sprite_smoke_test.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ppu.h"        // ppu_render_argb8888
#include "ppu_mem.h"    // ppu_mem_write
#include "ppu_regs.h"   // ppu_regs_write, ppu_regs_oam_poke

#ifndef NES_W
#define NES_W 256
#endif
#ifndef NES_H
#define NES_H 240
#endif

static void clear_oam(void) {
    for (int i = 0; i < 256; ++i) ppu_regs_oam_poke((uint8_t)i, 0);
}

/* Solid 8x8 tile with every pixel = px (0..3) at 'base' (0x0000 or 0x1000) */
static void write_solid_tile(uint16_t base, uint8_t tile_index, uint8_t px) {
    uint8_t lo = (px & 1) ? 0xFF : 0x00;
    uint8_t hi = (px & 2) ? 0xFF : 0x00;
    for (int row = 0; row < 8; ++row) {
        ppu_mem_write((uint16_t)(base + tile_index * 16 + row),      lo);
        ppu_mem_write((uint16_t)(base + tile_index * 16 + 8 + row), hi);
    }
}

static void write_sprite_palette0(uint8_t c1, uint8_t c2, uint8_t c3) {
    ppu_mem_write(0x3F00, 0x22); // backdrop (bluish)
    ppu_mem_write(0x3F11, c1);
    ppu_mem_write(0x3F12, c2);
    ppu_mem_write(0x3F13, c3);
}

static int run_case(int sprites_8x16) {
    write_sprite_palette0(0x16, 0x27, 0x30);

    // Use tile 0 (and tile 1 if 8x16) at base $0000
    write_solid_tile(0x0000, 0, 2);
    if (sprites_8x16) write_solid_tile(0x0000, 1, 2);

    // PPUCTRL: bit5 = sprite size (8x16), bit3 = sprite pattern table (0)
    uint8_t ctrl = (sprites_8x16 ? 0x20 : 0x00);
    ppu_regs_write(0, ctrl);

    // One sprite at (60,40): OAM = {Y, tile, attr, X}
    clear_oam();
    ppu_regs_oam_poke(0, 40);  // Y
    ppu_regs_oam_poke(1, 0);   // tile index
    ppu_regs_oam_poke(2, 0);   // attributes: palette 0, no flip
    ppu_regs_oam_poke(3, 60);  // X

    static uint32_t fb[NES_W * NES_H];
    memset(fb, 0, sizeof(fb));
    ppu_render_argb8888(fb, NES_W * 4);

    uint32_t bg = fb[0];

    int sx = 60 + 4;
    int sy = 40 + 1 + (sprites_8x16 ? 8 : 4);
    int pitch_px = NES_W;

    uint32_t p_in   = fb[sy * pitch_px + sx];
    uint32_t p_left = fb[(40 + 1) * pitch_px + (60 - 2)];
    uint32_t p_abov = fb[(40 - 2) * pitch_px + (60 + 4)];

    int pass = 1;
    if (p_in == bg)   { fprintf(stderr, "FAIL: sprite pixel equals background (inside)\n"); pass = 0; }
    if (p_left != bg) { fprintf(stderr, "FAIL: left outside pixel not background\n"); pass = 0; }
    if (p_abov != bg) { fprintf(stderr, "FAIL: above outside pixel not background\n"); pass = 0; }

    if (sprites_8x16) {
        int sy_low = 40 + 1 + 12;
        uint32_t p_low = fb[sy_low * pitch_px + sx];
        if (p_low == bg) { fprintf(stderr, "FAIL: 8x16 lower half not drawn\n"); pass = 0; }
    }

    fprintf(stderr, "Sprite smoke (%s): %s\n",
            sprites_8x16 ? "8x16" : "8x8", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int main(void) {
    int rc = 0;
    rc |= run_case(0);  // 8x8
    rc |= run_case(1);  // 8x16
    return rc;
}
