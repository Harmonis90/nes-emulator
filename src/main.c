// src/main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "nes.h"
#include "cpu.h"         // for cpu_get_cycles() if you want extra stats
#include "ppu.h"         // optional
#include "cartridge.h"   // fallback if nes_load_rom_file isn't available

static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s <rom.nes> [-f frames] [-s seconds]\n"
        "  exactly one of -f or -s may be given. if neither, runs 1 frame.\n"
        "examples:\n"
        "  %s nestest.nes -f 60     # run 60 frames\n"
        "  %s nestest.nes -s 1.0    # run ~1 second\n",
        prog, prog, prog
    );
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *rom_path = argv[1];
    int   have_frames = 0;
    int   frames = 1;
    int   have_seconds = 0;
    double seconds = 0.0;

    // parse options
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            frames = (int)strtol(argv[++i], NULL, 10);
            have_frames = 1;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            seconds = strtod(argv[++i], NULL);
            have_seconds = 1;
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (have_frames && have_seconds) {
        fprintf(stderr, "error: choose either -f or -s, not both.\n");
        return 1;
    }
    if (!have_frames && !have_seconds) {
        // default: run 1 frame
        frames = 1;
        have_frames = 1;
    }

    // --- boot the console ---
    nes_init();

    // load ROM (prefer nes_load_rom_file if you kept that API)
#if defined(__has_include)
# if __has_include("nes.h")
    if (nes_load_rom_file(rom_path) == 0) {
        fprintf(stderr, "failed to load ROM: %s\n", rom_path);
        return 1;
    }
# else
    if (cartridge_load_file(rom_path) != 0) {
        fprintf(stderr, "failed to load ROM: %s\n", rom_path);
        return 1;
    }
# endif
#else
    // If your toolchain lacks __has_include, call cartridge loader directly:
    if (cartridge_load_file(rom_path) != 0) {
        fprintf(stderr, "failed to load ROM: %s\n", rom_path);
        return 1;
    }
#endif

    nes_reset();

    // --- run ---
    if (have_frames) {
        if (frames < 0) frames = 0;
        for (int i = 0; i < frames; ++i) {
            uint64_t f = nes_step_frame();
            // comment out if too chatty:
            //printf("frame %llu\n", (unsigned long long)f);
        }
    } else {
        if (seconds < 0.0) seconds = 0.0;
        nes_step_seconds(seconds);
        printf("ran ~%.3f seconds, frame=%llu cycles=%llu\n",
               seconds,
               (unsigned long long)nes_frame_count(),
               (unsigned long long)cpu_get_cycles());
    }

    nes_shutdown();
    return 0;
}
