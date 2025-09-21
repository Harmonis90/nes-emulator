#include <stdint.h>
#include <stdbool.h>

#include "ppu.h"
#include "ppu_regs.h"


// NTSC-ish timings (simplified):
// 341 PPU dots/line, 262 scanlines/frame.
// VBlank starts at scanline 241, dot 1; ends at pre-render line (261), dot 1.

static int ppu_dot = 0; // 0 .. 340
static int ppu_scanline = 0; // 0 .. 261

static inline void ppu_advance_dot(void)
{
    ppu_dot++;
    if (ppu_dot >= 341)
    {
        ppu_dot = 0;
        ppu_scanline++;
        if (ppu_scanline >= 262)
        {
            ppu_scanline = 0;
        }
        // vblank transitions occur at dot 1 of specific scanlines, but we can drive on line boundary:
        if (ppu_scanline == 241)
        {
            // entering vblank
            ppu_regs_set_vblank(true);
        }
        else if (ppu_scanline == 261)
        {
            // leaving vblank (pre-render)
            ppu_regs_set_vblank(false);
        }
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