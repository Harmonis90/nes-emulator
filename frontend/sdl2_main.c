#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>               // <-- add this
#include "nes.h"              // nes_load_rom_file, nes_reset, nes_step_frame, ...
#include "sdl2_frontend.h"    // Sdl2Frontend API

// NTSC NES runs ~60.0988 fps => ~16.639 ms per frame
static inline void throttle_60hz(uint64_t frame_start_ticks) {
    const double target_ms = 1000.0 / 60.0988;
    const uint64_t freq = SDL_GetPerformanceFrequency();
    for (;;) {
        uint64_t now = SDL_GetPerformanceCounter();
        double elapsed_ms = 1000.0 * (double)(now - frame_start_ticks) / (double)freq;
        if (elapsed_ms >= target_ms) break;

        // Sleep most of the remainder (coarse), then busy-wait the tail for smoother pacing.
        double remain_ms = target_ms - elapsed_ms;
        if (remain_ms > 2.0) {
            SDL_Delay((Uint32)(remain_ms - 1.0));
        } else {
            // short busy wait
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s rom.nes [-scale N]\n", argv[0]);
        return 1;
    }
    int scale = 3;
    for (int i=2;i<argc;i++)
        if (!strcmp(argv[i], "-scale") && i+1<argc) scale = atoi(argv[++i]);

    if (!nes_load_rom_file(argv[1])) {       // loader: 1 on success
        fprintf(stderr, "nes_load_rom_file failed: %s\n", argv[1]);
        return 1;
    }
    nes_reset();
    SDL_SetMainReady();           // <-- required when SDL_MAIN_HANDLED
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    Sdl2Frontend* fe = NULL;
    // Turn VSYNC off when you use manual throttle, to avoid “double throttling”.
    Sdl2Config cfg = { .title = "NES Emulator (SDL2)", .scale = scale, .vsync = 0, .integer_scale = 1 };
    if (!sdl2_frontend_create(&cfg, &fe)) return 1;

    while (sdl2_frontend_pump(fe)) {
        uint64_t t0 = SDL_GetPerformanceCounter();  // timestamp frame start

        nes_step_frame();               // advance exactly one NES frame
        sdl2_frontend_present(fe);      // upload + present framebuffer

        throttle_60hz(t0);              // <-- throttle to ~16.639 ms per frame
    }

    sdl2_frontend_destroy(fe);
    return 0;
}
