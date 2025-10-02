// src/ppu/ppu_render.c
// Full background + sprite renderer (ARGB8888), no external helpers required.
// Uses ppu_mem_read/ppu_regs_get_scroll/ppu_ctrl_reg/ppu_mask_reg/ppu_oam_data.
//
// Debug toggles (set to 0 for accuracy):
#ifndef FORCE_SPRITES_ON_TOP
#define FORCE_SPRITES_ON_TOP 0   // 1: ignore sprite priority & PPUMASK.sprites
#endif
#ifndef IGNORE_LEFT8_BG_CLIP
#define IGNORE_LEFT8_BG_CLIP 0   // 1: ignore PPUMASK bit1 (BG left 8 clip)
#endif
#ifndef IGNORE_LEFT8_SPR_CLIP
#define IGNORE_LEFT8_SPR_CLIP 0  // 1: ignore PPUMASK bit2 (SPR left 8 clip)
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ppu.h"
#include "ppu_regs.h"
#include "ppu_mem.h"

// Local 64-entry ARGB8888 palette (Nestopia-ish). Avoids linking issues.
static const uint32_t PALETTE_ARGB[64] = {
  0xFF666666,0xFF002A88,0xFF1412A7,0xFF3B00A4,0xFF5C007E,0xFF6E0040,0xFF6C0600,0xFF561D00,
  0xFF333500,0xFF0B4800,0xFF005200,0xFF004F08,0xFF00404D,0xFF000000,0xFF000000,0xFF000000,
  0xFFADADAD,0xFF155FD9,0xFF4240FF,0xFF7527FE,0xFFA01ACC,0xFFB71E7B,0xFFB53120,0xFF994E00,
  0xFF6B6D00,0xFF388700,0xFF0E9300,0xFF008F32,0xFF007C8D,0xFF000000,0xFF000000,0xFF000000,
  0xFFFFFEFF,0xFF64B0FF,0xFF9290FF,0xFFC676FF,0xFFF36AFF,0xFFFE6ECC,0xFFFE8170,0xFFEA9E22,
  0xFFBCBE00,0xFF88D800,0xFF5CE430,0xFF45E082,0xFF48CDDE,0xFF4F4F4F,0xFF000000,0xFF000000,
  0xFFFFFEFF,0xFFC0DFFF,0xFFD3D2FF,0xFFE8C8FF,0xFFFBC2FF,0xFFFFC4EA,0xFFFFCCCB,0xFFF7D8A5,
  0xFFE4E594,0xFFCFEF96,0xFFBDF4AB,0xFFB3F3CC,0xFFB5EBF2,0xFFB8B8B8,0xFF000000,0xFF000000
};

#define NES_W 256
#define NES_H 240

// Palette index normalizer for $3F00-$3F1F region with mirroring.
// Also maps $3F10/$14/$18/$1C -> $3F00/$04/$08/$0C.
static inline uint16_t pal_index(uint16_t addr)
{
    addr = (uint16_t)(0x3F00u | (addr & 0x1Fu));
    if ((addr & 0x13u) == 0x10u) addr = (uint16_t)(addr & ~0x10u);
    return addr;
}

static inline uint8_t bitpair(uint8_t lo, uint8_t hi, int bit /*0..7 left..right*/)
{
    int s = 7 - bit;
    return (uint8_t)(((lo >> s) & 1u) | (((hi >> s) & 1u) << 1));
}
static inline uint8_t bitpair_flipped(uint8_t lo, uint8_t hi, int bit /*0..7*/, bool hflip)
{
    int s = hflip ? bit : (7 - bit);
    return (uint8_t)(((lo >> s) & 1u) | (((hi >> s) & 1u) << 1));
}

// --- Background renderer (with scroll) ---
static void draw_background_scrolled(uint32_t* dst, uint8_t* bg_opaque,
                                     int pitch_px, uint8_t ctrl, uint8_t mask)
{
    const bool show_bg   = (mask & 0x08) != 0;
    const bool clip_bg8  = ((mask & 0x02) == 0) && (IGNORE_LEFT8_BG_CLIP == 0);

    // If BG disabled, nothing to do (backdrop already filled)
    if (!show_bg) return;

    // Scroll state from registers: t (VRAM temp), fine X
    uint16_t t;
    uint8_t fine_x;
    ppu_regs_get_scroll(&t, &fine_x);

    const uint8_t  coarse_x0 = (uint8_t)(t & 0x1F);
    const uint8_t  coarse_y0 = (uint8_t)((t >> 5) & 0x1F);
    const uint8_t  nt_bits   = (uint8_t)((t >> 10) & 0x03); // base nametable quadrant
    const uint8_t  fine_y0   = (uint8_t)((t >> 12) & 0x07);

    // Background pattern table base: PPUCTRL bit 4 (0x10)
    const uint16_t bg_tbl_base = (ctrl & 0x10) ? 0x1000u : 0x0000u;

    // We'll render pixel-by-pixel with scroll wrap across the 2x2 nametable space (512x480).
    for (int sy = 0; sy < NES_H; ++sy)
    {
        uint16_t world_y = (uint16_t)(coarse_y0 * 8u + fine_y0 + sy); // 0..(480-1)
        uint8_t  nt_y    = (uint8_t)((world_y / 240u) & 1u);
        uint8_t  tile_y  = (uint8_t)((world_y % 240u) / 8u);
        uint8_t  fine_y  = (uint8_t)(world_y & 7u);

        // Precompute row pattern bytes for each tile across the line to reduce ppu_mem_read calls
        int last_tile_x = -1;
        uint8_t row_lo = 0, row_hi = 0;
        uint8_t pal_cache[32]; // palette index (0..3) per tile

        // Build palettes for the 32 tile columns (with nametable wrap)
        for (int tx = 0; tx < 32; ++tx)
        {
            uint16_t world_x_for_tile = (uint16_t)(coarse_x0 * 8u + fine_x + tx*8u);
            uint8_t  nt_x   = (uint8_t)((world_x_for_tile / 256u) & 1u);
            uint8_t  tile_x = (uint8_t)((world_x_for_tile % 256u) / 8u);

            // Resolve base nametable from nt_bits plus nt_x/nt_y
            uint8_t nt_quadrant = (uint8_t)(((nt_bits & 1u) ^ nt_x) | (((nt_bits >> 1) ^ nt_y) << 1));
            uint16_t nt_base = (uint16_t)(0x2000u + (uint16_t)nt_quadrant * 0x400u);

            // Name table tile index
            uint16_t name_addr = (uint16_t)(nt_base + tile_y * 32u + tile_x);
            uint8_t  tile_index = ppu_mem_read(name_addr);

            // Attribute table
            uint16_t attr_addr = (uint16_t)(nt_base + 0x3C0u + (tile_y / 4u) * 8u + (tile_x / 4u));
            uint8_t  attr = ppu_mem_read(attr_addr);
            int shift = ((tile_y & 2) << 1) | (tile_x & 2); // 0,2,4,6
            pal_cache[tx] = (uint8_t)((attr >> shift) & 0x03);

            // Preload row bytes for the first tile; others will be loaded in-column loop
            if (tx == 0) {
                uint16_t pat = (uint16_t)(bg_tbl_base + (uint16_t)tile_index * 16u + fine_y);
                row_lo = ppu_mem_read(pat + 0);
                row_hi = ppu_mem_read(pat + 8);
                last_tile_x = 0;
            }
        }

        for (int sx = 0; sx < NES_W; ++sx)
        {
            uint16_t world_x = (uint16_t)(coarse_x0 * 8u + fine_x + sx);
            uint8_t  nt_x    = (uint8_t)((world_x / 256u) & 1u);
            uint8_t  tile_x  = (uint8_t)((world_x % 256u) / 8u);
            uint8_t  px_in_tile = (uint8_t)(world_x & 7u);

            // Resolve NT quadrant consistent with the cache (same formula)
            uint8_t nt_quadrant = (uint8_t)(((nt_bits & 1u) ^ nt_x) | (((nt_bits >> 1) ^ ((world_y / 240u) & 1u)) << 1));
            uint16_t nt_base = (uint16_t)(0x2000u + (uint16_t)nt_quadrant * 0x400u);

            // Load tile row pattern bytes if we crossed into a new tile column
            if (tile_x != last_tile_x)
            {
                uint16_t name_addr = (uint16_t)(nt_base + tile_y * 32u + tile_x);
                uint8_t tile_index = ppu_mem_read(name_addr);
                uint16_t pat = (uint16_t)(bg_tbl_base + (uint16_t)tile_index * 16u + fine_y);
                row_lo = ppu_mem_read(pat + 0);
                row_hi = ppu_mem_read(pat + 8);
                last_tile_x = tile_x;
            }

            uint8_t pix = bitpair(row_lo, row_hi, px_in_tile); // 0..3
            bool left8 = (sx < 8);
            if ((clip_bg8 && left8) || pix == 0) {
                // transparent BG here; leave backdrop pixel and mark not opaque
                bg_opaque[sy * NES_W + sx] = 0;
                continue;
            }

            uint8_t pal = pal_cache[tile_x]; // 0..3
            uint16_t paddr = (uint16_t)(0x3F00u + pal * 4u + pix); // pix!=0 here
            uint8_t cidx = ppu_mem_read(pal_index(paddr));
            dst[sy * pitch_px + sx] = PALETTE_ARGB[cidx & 0x3F];
            bg_opaque[sy * NES_W + sx] = 1;
        }
    }
}

// --- Sprite overlay (8x8 & 8x16) ---
static void draw_sprites(uint32_t* dst, const uint8_t* bg_opaque,
                         int pitch_px, uint8_t ctrl, uint8_t mask)
{
    const bool show_spr  = ((mask & 0x10) != 0) || (FORCE_SPRITES_ON_TOP != 0);
    if (!show_spr) return;

    const bool clip_spr8 = ((mask & 0x04) == 0) && (IGNORE_LEFT8_SPR_CLIP == 0);
    const bool mode_8x16 = (ctrl & 0x20) != 0;

    // ✅ Correct sprite table bit in 8×8 mode: PPUCTRL bit 3 (0x08)
    const uint16_t spr_tbl_8x8 = (ctrl & 0x08) ? 0x1000u : 0x0000u;

    const uint8_t* OAM = ppu_oam_data();
    const bool respect_priority = (FORCE_SPRITES_ON_TOP == 0);

    for (int i = 0; i < 64; ++i)
    {
        uint8_t y    = OAM[i*4 + 0];
        uint8_t tile = OAM[i*4 + 1];
        uint8_t attr = OAM[i*4 + 2];
        uint8_t x    = OAM[i*4 + 3];

        int spr_y = (int)y + 1;             // NES Y+1 rule
        if (spr_y >= NES_H) continue;       // offscreen (e.g., F8)

        const bool vflip = (attr & 0x80) != 0;
        const bool hflip = (attr & 0x40) != 0;
        const bool behind_bg = (attr & 0x20) != 0;
        const uint8_t palset = (uint8_t)(attr & 0x03);

        if (!mode_8x16)
        {
            // ---- 8×8 ----
            uint16_t base = (uint16_t)(spr_tbl_8x8 + (uint16_t)tile * 16u);
            for (int row = 0; row < 8; ++row)
            {
                int pr = vflip ? (7 - row) : row;
                uint8_t lo = ppu_mem_read((uint16_t)(base + pr + 0));
                uint8_t hi = ppu_mem_read((uint16_t)(base + pr + 8));
                int yy = spr_y + row; if ((unsigned)yy >= NES_H) continue;

                for (int col = 0; col < 8; ++col)
                {
                    int xx = (int)x + col;
                    if ((unsigned)xx >= NES_W) continue;
                    if (clip_spr8 && xx < 8) continue;

                    uint8_t pix = bitpair_flipped(lo, hi, col, hflip);
                    if (pix == 0) continue;

                    if (respect_priority && behind_bg) {
                        if (bg_opaque && bg_opaque[yy * NES_W + xx]) continue;
                        if (!bg_opaque) continue; // approximate if no mask
                    }

                    // Sprite palettes at $3F10 + pal*4 + (1..3)
                    uint16_t paddr = (uint16_t)(0x3F10u + (uint16_t)palset * 4u + pix);
                    uint8_t  cidx  = ppu_mem_read(pal_index(paddr));
                    dst[yy * pitch_px + xx] = PALETTE_ARGB[cidx & 0x3F];
                }
            }
        }
        else
        {
            // ---- 8×16 ----
            const uint16_t table = (tile & 1) ? 0x1000u : 0x0000u;
            for (int row = 0; row < 16; ++row)
            {
                int pr = vflip ? (15 - row) : row;
                int half = (pr >= 8);
                uint8_t tindex = (uint8_t)((tile & 0xFE) + half);
                uint16_t base  = (uint16_t)(table + (uint16_t)tindex * 16u);
                int subrow     = pr & 7;

                uint8_t lo = ppu_mem_read((uint16_t)(base + subrow + 0));
                uint8_t hi = ppu_mem_read((uint16_t)(base + subrow + 8));
                int yy = spr_y + row; if ((unsigned)yy >= NES_H) continue;

                for (int col = 0; col < 8; ++col)
                {
                    int xx = (int)x + col;
                    if ((unsigned)xx >= NES_W) continue;
                    if (clip_spr8 && xx < 8) continue;

                    uint8_t pix = bitpair_flipped(lo, hi, col, hflip);
                    if (pix == 0) continue;

                    if (respect_priority && behind_bg) {
                        if (bg_opaque && bg_opaque[yy * NES_W + xx]) continue;
                        if (!bg_opaque) continue;
                    }

                    uint16_t paddr = (uint16_t)(0x3F10u + (uint16_t)palset * 4u + pix);
                    uint8_t  cidx  = ppu_mem_read(pal_index(paddr));
                    dst[yy * pitch_px + xx] = PALETTE_ARGB[cidx & 0x3F];
                }
            }
        }
    }
}

// --- Public entry point: fill backdrop, draw BG, then sprites ---
void ppu_render_argb8888(uint32_t* dst, int pitch_bytes)
{
    if (!dst || pitch_bytes <= 0) return;
    const int pitch_px = pitch_bytes / 4;

    // 1) Fill backdrop with universal background color ($3F00)
    uint8_t bg_idx = ppu_mem_read(pal_index(0x3F00));
    uint32_t back_color = PALETTE_ARGB[bg_idx & 0x3F];
    for (int y = 0; y < NES_H; ++y) {
        uint32_t* row = dst + y * pitch_px;
        for (int x = 0; x < NES_W; ++x) row[x] = back_color;
    }

    const uint8_t ctrl = ppu_ctrl_reg();
    const uint8_t mask = ppu_mask_reg();

    // 2) Background (tracks an opacity buffer for sprite priority)
    static uint8_t bg_opaque[NES_W * NES_H];
    memset(bg_opaque, 0, sizeof(bg_opaque));
    draw_background_scrolled(dst, bg_opaque, pitch_px, ctrl, mask);

    // 3) Sprites on top
    draw_sprites(dst, bg_opaque, pitch_px, ctrl, mask);
}
