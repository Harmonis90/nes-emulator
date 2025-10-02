//
// Created by Seth on 8/30/2025.
//
#ifndef NES_PPU_H
#define NES_PPU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

    // ----------------------------------------------------------------------------
    // Core PPU entry points
    // ----------------------------------------------------------------------------

    // Initialize/reset the PPU state (call at power-on/reset)
    void     ppu_reset(void);

    // CPU-visible register access ($2000–$3FFF, mirrored every 8 bytes)
    uint8_t  ppu_read(uint16_t cpu_addr);
    void     ppu_write(uint16_t cpu_addr, uint8_t value);

    // Optional: enable/disable “fake vblank” (used by some early tests)
    void     ppu_set_fake_vblank(int on);

    // True while PPUSTATUS bit 7 (VBlank) is set
    bool     ppu_in_vblank(void);

    // Optional: advance PPU timing; call with CPU cycles elapsed (if used)
    void     ppu_step(int cpu_cycles);

    // PPU timing helpers
    void     ppu_timing_reset(void);
    uint64_t ppu_frame_count(void);

    // Renderer — writes a full ARGB8888 frame (256x240)
    void     ppu_render_argb8888(uint32_t* dst, int pitch_bytes);

    // ----------------------------------------------------------------------------
    // Lightweight debug / stats (used by tests)
    // Implemented in ppu_regs.c; exposed here so tests only need ppu.h
    // ----------------------------------------------------------------------------
    int      ppu_dma_count(void);             // number of $4014 DMA ops performed
    int      ppu_oamaddr_write_count(void);   // writes to $2003
    int      ppu_oamdata_write_count(void);   // writes to $2004
    int      ppu_nmi_count(void);             // NMIs actually fired
    uint8_t  ppu_ppuctrl_get(void);           // current $2000 latch
    uint8_t  ppu_ppustatus_get(void);         // current $2002 latch (no side effects)

#ifdef __cplusplus
}
#endif

#endif // NES_PPU_H
