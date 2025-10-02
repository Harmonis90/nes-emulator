#include <string.h>
#include <SDL.h>
#include "apu.h"
#include "sdl2_audio.h"

static SDL_AudioDeviceID g_dev = 0;
static uint32_t g_buffer_frames = 1024;        // configurable before init
static int g_bytes_per_frame = sizeof(int16_t); // mono S16

// SDL callback: pull samples from APU ring buffer, zero-fill remainder.
static void sdl_audio_cb(void* userdata, Uint8* stream, int len_bytes) {
    (void)userdata;

    const size_t want_frames = (size_t)(len_bytes / g_bytes_per_frame);
    // Scratch buffer sized to max callback ask; 8192 is plenty for typical sizes.
    static int16_t scratch[8192];

    size_t got = apu_read_samples(scratch, want_frames);

    // Copy what we got
    if (got) {
        memcpy(stream, scratch, got * g_bytes_per_frame);
    }
    // Zero any remainder to avoid buzz
    if (got < want_frames) {
        memset(stream + got * g_bytes_per_frame, 0, (want_frames - got) * g_bytes_per_frame);
    }
}

void sdl2_audio_set_buffer_frames(uint32_t frames) {
    if (frames == 0) frames = 1024;
    g_buffer_frames = frames;
}

bool sdl2_audio_init(void) {
    if (g_dev) return true; // already init

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            SDL_Log("SDL audio init failed: %s", SDL_GetError());
            return false;
        }
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    SDL_zero(have);

    want.freq     = 48000;            // matches APU default; device may change it
    want.format   = AUDIO_S16SYS;     // 16-bit signed, native endianness
    want.channels = 1;                // mono
    want.samples  = (Uint16)g_buffer_frames; // callback buffer in frames
    want.callback = sdl_audio_cb;

    // Open default device for playback
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) {
        SDL_Log("SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    // Keep APU rate in sync with the actual device rate
    if (have.freq > 0) {
        apu_set_sample_rate((uint32_t)have.freq);
    }
    // Sanity: format/channels are what we asked; if a platform changes them, adapt
    if (have.channels != 1 || have.format != AUDIO_S16SYS) {
        SDL_Log("Warning: got unexpected audio format; channels=%d fmt=0x%X. Adapting.",
                have.channels, (unsigned)have.format);
        // If you want to support other formats later, add conversion here.
    }

    g_bytes_per_frame = (int)SDL_AUDIO_BITSIZE(have.format) / 8 * have.channels;

    SDL_PauseAudioDevice(g_dev, 0); // start callback
    return true;
}

void sdl2_audio_pause(int pause_on) {
    if (!g_dev) return;
    SDL_PauseAudioDevice(g_dev, pause_on ? 1 : 0);
}

void sdl2_audio_shutdown(void) {
    if (g_dev) {
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
    }
    // (We don’t quit the SDL audio subsystem here; leave that to your app shutdown.)
}
