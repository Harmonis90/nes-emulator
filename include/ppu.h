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

// Initialize/reset the PPU stub state (call at power-on/reset)
void ppu_reset(void);

// CPU-visible register access ($2000–$3FFF, mirrored every 8 bytes)
uint8_t ppu_read(uint16_t cpu_addr);
void ppu_write(uint16_t cpu_addr, uint8_t value);

// Optional: enable/disable “fake vblank” so many ROMs can leave init loops.
// When enabled (default), $2002’s VBlank bit will be set frequently.
void ppu_set_fake_vblank(int on);

bool ppu_in_vblank(void);
// Optional: advance PPU timing; call periodically with CPU cycles elapsed.
// In the stub, this just re-asserts VBlank after a short delay when enabled.
void ppu_step(int cpu_cycles);

// DMA: copy 256 bytes from CPU page (page<<8) into OAM[0..255]
void ppu_oam_dma(uint8_t page);

// Direct OAM accessors (handy for unit tests)
void ppu_oam_write_byte(uint8_t index, uint8_t val);
uint8_t ppu_oam_read_byte(uint8_t index);
uint8_t const* ppu_oam_data(void); // read only pointer for debug/tests

// ppu timing functions
void ppu_timing_reset(void);
uint64_t ppu_frame_count(void);
#ifdef __cplusplus
}
#endif

#endif //NES_PPU_H
