#pragma once
#include <stdbool.h>
#include <stdint.h>

// Initialize SDL2 audio and start playback.
// Returns false on failure. On success, APU sample rate is synced to the device.
bool sdl2_audio_init(void);

// Stop playback and close the device.
void sdl2_audio_shutdown(void);

// Optional: pause/resume audio callback.
void sdl2_audio_pause(int pause_on);

// Optional: change buffer size (in frames) next init; call before init.
// (Defaults to 1024 if never set.)
void sdl2_audio_set_buffer_frames(uint32_t frames);
