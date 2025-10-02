#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

/* Core constants */
enum {NES_W = 256, NES_H = 240};

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

/* Video: expose a persistent 256x240 ARGB8888 buffer for frontends */
const uint32_t* nes_framebuffer_argb8888(int* out_pitch_bytes);

/* Input: one byte per pad (A,B,Select,Start,Up,Down,Left,Right) */
void nes_set_controller_state(int pad_index, uint8_t state);

#ifdef __cplusplus
}
#endif