//
// Created by Seth on 8/30/2025.
//

#ifndef NES_PPU_H
#define NES_PPU_H

#include <stdint.h>

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

// Optional: advance PPU timing; call periodically with CPU cycles elapsed.
// In the stub, this just re-asserts VBlank after a short delay when enabled.
void ppu_step(int cpu_cycles);

#ifdef __cplusplus
}
#endif

#endif //NES_PPU_H
