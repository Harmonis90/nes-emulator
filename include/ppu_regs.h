#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ppu_mem.h"

#ifdef __cplusplus
extern "C"{
#endif

// ----------------------------------------------------------------------------
// Power-on/reset
// ----------------------------------------------------------------------------
void     ppu_regs_reset(void);

// ----------------------------------------------------------------------------
// CPU-visible PPU registers ($2000-$2007, mirrored every 8)
// Pass either lo3 (0..7) or a full CPU address in $2000-$3FFF
// ----------------------------------------------------------------------------
uint8_t  ppu_regs_read(uint16_t addr);
void     ppu_regs_write(uint16_t addr, uint8_t data);

// ----------------------------------------------------------------------------
// Cartridge/mapper-controlled nametable mirroring
// ----------------------------------------------------------------------------
static inline void ppu_regs_set_mirroring(mirroring_t m)
{
    ppu_mem_set_mirroring(m);
}

// Current latched registers (peek helpers)
uint8_t  ppu_regs_status_peek(void);  // $2002 without side-effects
uint8_t  ppu_ctrl_reg(void);          // $2000 latch
uint8_t  ppu_mask_reg(void);          // $2001 latch

// ----------------------------------------------------------------------------
// VBlank / NMI control (minimal)
// Sets/clears VBlank in PPUSTATUS. If NMI was enabled in PPUCTRL (bit 7),
// an NMI will be raised immediately on VBlank set or when enabling NMI
// while already in VBlank.
// ----------------------------------------------------------------------------
void     ppu_regs_set_vblank(bool on);

// Convenience shorthands
static inline void ppu_regs_fake_vblank(void)  { ppu_regs_set_vblank(true); }
static inline void ppu_regs_clear_vblank(void) { ppu_regs_set_vblank(false); }

// ----------------------------------------------------------------------------
// OAM access (useful for tests)
// ----------------------------------------------------------------------------
void     ppu_regs_oam_clear(void);         // (optional) clear OAM to 0 if you have it
void     ppu_oam_dma(uint8_t page);        // called on CPU write to $4014
uint8_t  ppu_regs_oam_peek(int index);     // OAM[index], no increment; asserts range in debug
void     ppu_regs_oam_poke(int index, uint8_t v);
uint8_t const* ppu_oam_data(void);         // read-only pointer for debug/tests

// Expose scroll/toggle state to renderer
void     ppu_regs_get_scroll(uint16_t* t_out, uint8_t* fine_x_out);

// ----------------------------------------------------------------------------
// Optional debug counters (implemented in ppu_regs.c)
// ----------------------------------------------------------------------------
int      ppu_dma_count(void);
int      ppu_oamaddr_write_count(void);
int      ppu_oamdata_write_count(void);
int      ppu_nmi_count(void);

// Also expose these here for low-level tools (duplicates in ppu.h for tests)
uint8_t  ppu_ppuctrl_get(void);
uint8_t  ppu_ppustatus_get(void);


uint8_t ppu_last_dma_page(void);
uint8_t ppu_oamaddr_peek(void);

#ifdef __cplusplus
}
#endif
