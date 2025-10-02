#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

#include "ppu.h"
#include "ppu_regs.h"

// Forward decl from mapper_mmc3.c (level IRQ version).
// This must be linked in when Mapper 4 is active.
extern void mapper_mmc3_on_ppu_scanline_tick(void);

#ifndef PPU_TRACE
#define PPU_TRACE 0
#endif

// NTSC-ish timings (simplified):
// 341 PPU dots/line, 262 scanlines/frame.
// VBlank starts at scanline 241, dot 1; ends at pre-render line (261), dot 1.

static uint64_t ppu_frame_ctr = 0;

static int ppu_dot = 0;       // 0 .. 340
static int ppu_scanline = 0;  // 0 .. 261

uint64_t ppu_frame_count(void)
{
    return ppu_frame_ctr;
}

void ppu_timing_reset(void)
{
    ppu_dot = 0;
    ppu_scanline = 0;
    ppu_frame_ctr = 0;
}

static inline void ppu_advance_dot(void)
{
    // advance one PPU dot
    ppu_dot++;
    if (ppu_dot == 341) {
        ppu_dot = 0;
        ppu_scanline++;
        if (ppu_scanline == 262) {
            ppu_scanline = 0;
            ppu_frame_ctr++;
#if PPU_TRACE
            fprintf(stderr, "[PPU] frame start #%" PRIu64 " (pre-render)\n", ppu_frame_ctr);
#endif
        }
    }

    // ---- MMC3 scanline IRQ hook ----
    // Call once per visible scanline at dot ~260 when rendering is enabled.
    // This approximates a valid A12 rising edge and keeps MMC3 IRQs firing
    // even if CHR fetches aren't modeled per PPU cycle.
    if (ppu_dot == 260 && ppu_scanline >= 0 && ppu_scanline < 240) {
        uint8_t mask = ppu_mask_reg();
        if (mask & 0x18) { // BG or SPR enabled
            mapper_mmc3_on_ppu_scanline_tick();
#if PPU_TRACE
            fprintf(stderr, "[PPU] MMC3 tick sl=%d dot=260 mask=%02X\n", ppu_scanline, mask);
#endif
        }
    }

    // vblank transitions at dot 1 of specific scanlines
    if (ppu_dot == 1 && ppu_scanline == 241) {
        ppu_regs_set_vblank(true);
#if PPU_TRACE
        fprintf(stderr, "[PPU] VBL SET at frame=%" PRIu64 ", sl=241, dot=1\n", ppu_frame_ctr);
#endif
    }
    if (ppu_dot == 1 && ppu_scanline == 261) {
        ppu_regs_set_vblank(false);
#if PPU_TRACE
        fprintf(stderr, "[PPU] VBL CLR at frame=%" PRIu64 ", sl=261, dot=1\n", ppu_frame_ctr);
#endif
    }
}

void ppu_step(int cpu_cycles)
{
    // PPU runs 3x CPU speed
    int ppu_cycles = cpu_cycles * 3;
    while (ppu_cycles-- > 0)
    {
        ppu_advance_dot();
        // (later: sprite eval, fetch pipeline, sprite 0 hit/overflow, etc.)
    }
}
