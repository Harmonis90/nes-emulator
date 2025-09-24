#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

// Initialize subsystems that don't depend on a ROM yet.
void nes_init(void);

// Reset the whole console (CPU, PPU, APU stubs, bus).
void nes_reset(void);

int nes_load_rom_file(const char *path); // optional

// Step until the next vblank edge (one full frame). Returns the frame index.
uint64_t nes_step_frame(void);

uint64_t nes_run_frames(uint32_t count);

void nes_step_seconds(double seconds);

// Optional: expose the running frame count.
uint64_t nes_frame_count(void);

// Optional: shutdown hook.
void nes_shutdown(void);





#ifdef __cplusplus
}
#endif