#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ppu_mem.h"


#ifdef __cplusplus
extern "C"{
#endif


// --- Power-on/reset -----------------------------------------------------------
void ppu_regs_reset(void);

// --- CPU-visible PPU registers ($2000-$2007, mirrored every 8) ---------------
uint8_t ppu_regs_read(uint16_t addr);
void ppu_regs_write(uint16_t addr, uint8_t data);

// --- Cartridge/mapper controlled nametable mirroring -------------------------
static inline void ppu_regs_set_mirroring(mirroring_t m)
{
    ppu_mem_set_mirroring(m);
}

// --- Minimal vblank control (for early tests) --------------------------------
// Set/clear VBlank in PPUSTATUS. If NMI is enabled (PPUCTRL bit 7), will raise NMI.
void ppu_regs_set_vblank(bool on);

// Convenience shorthands
static inline void ppu_regs_fake_vblank(void) { ppu_regs_set_vblank(true); }
static inline void ppu_regs_clear_vblank(void) { ppu_regs_set_vblank(false); }

// --- Optional tiny OAM accessors (useful for tests) --------------------------
void ppu_regs_oam_clear(void);  // fill OAM with 0
void ppu_oam_dma(uint8_t page); // called when CPU writes $4014
uint8_t ppu_regs_oam_peek(uint8_t index); // non-incrementing peek
void ppu_regs_oam_poke(uint8_t index, uint8_t v);


#ifdef __cplusplus
}
#endif