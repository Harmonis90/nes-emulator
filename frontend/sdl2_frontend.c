// src/frontend/sdl2_frontend.c
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#include "nes.h"
#include "sdl2_frontend.h"
#include "sdl2_audio.h"

struct Sdl2Frontend
{
    SDL_Window* win;
    SDL_Renderer* ren;
    SDL_Texture* tex;
    SDL_GameController* gc;   // NEW: optional gamepad
    int running;
    int integer_scale;
};

/* Merge keyboard + (optional) gamepad state into controller 0 */
static uint8_t build_pad0_from_inputs(SDL_GameController* gc)
{
    int n = 0;
    SDL_PumpEvents(); // ensure scancodes are fresh before SDL_GetKeyboardState
    const Uint8* ks = SDL_GetKeyboardState(&n);
    uint8_t s = 0;

    // --- Keyboard bindings ---
    // Buttons
    if (ks[SDL_SCANCODE_Z])             s |= 1 << 0; // A
    if (ks[SDL_SCANCODE_X])             s |= 1 << 1; // B
    if (ks[SDL_SCANCODE_A])             s |= 1 << 2; // Select
    if (ks[SDL_SCANCODE_S])             s |= 1 << 3; // Start

    // Extra convenience keys
    if (ks[SDL_SCANCODE_RETURN])        s |= 1 << 3; // Start (Enter)
    if (ks[SDL_SCANCODE_KP_ENTER])      s |= 1 << 3; // Start (Keypad Enter)
    if (ks[SDL_SCANCODE_RSHIFT])        s |= 1 << 2; // Select (Right Shift)
    if (ks[SDL_SCANCODE_BACKSPACE])     s |= 1 << 2; // Select (Backspace)

    // D-pad
    if (ks[SDL_SCANCODE_UP])            s |= 1 << 4;
    if (ks[SDL_SCANCODE_DOWN])          s |= 1 << 5;
    if (ks[SDL_SCANCODE_LEFT])          s |= 1 << 6;
    if (ks[SDL_SCANCODE_RIGHT])         s |= 1 << 7;

    // --- Gamepad bindings (if present) ---
    if (gc && SDL_GameControllerGetAttached(gc)) {
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A))     s |= 1 << 0; // A
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_B))     s |= 1 << 1; // B
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK))  s |= 1 << 2; // Select
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_START)) s |= 1 << 3; // Start

        // D-pad (digital)
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP))    s |= 1 << 4;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  s |= 1 << 5;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  s |= 1 << 6;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) s |= 1 << 7;
    }

    return s;
}

int sdl2_frontend_create(const Sdl2Config* cfg, Sdl2Frontend** out_fe)
{
    if (!out_fe) return 0;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) return 0;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // nearest neighbor

    Sdl2Frontend* fe = (Sdl2Frontend*)calloc(1, sizeof(*fe));
    if (!fe) { SDL_Quit(); return 0; }

    const char* title = (cfg && cfg->title) ? cfg->title : "NES Emulator";
    int scale = (cfg && cfg->scale > 0) ? cfg->scale : 3;
    int vsync = (cfg && cfg->vsync) ? 1 : 0;
    fe->integer_scale = (cfg && cfg->integer_scale) ? 1 : 0;

    fe->win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               NES_W * scale, NES_H * scale, SDL_WINDOW_RESIZABLE);
    if (!fe->win) goto fail;

    Uint32 flags = SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    fe->ren = SDL_CreateRenderer(fe->win, -1, flags);
    if (!fe->ren) goto fail;

    SDL_RenderSetLogicalSize(fe->ren, NES_W, NES_H);
    SDL_RenderSetIntegerScale(fe->ren, fe->integer_scale ? SDL_TRUE : SDL_FALSE);

    // Use a streaming texture for cpu-side uploads
    fe->tex = SDL_CreateTexture(fe->ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NES_W, NES_H);
    if (!fe->tex) goto fail;

    // Try to open the first available game controller (optional)
    fe->gc = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            fe->gc = SDL_GameControllerOpen(i);
            if (fe->gc) break;
        }
    }
    // audio init
    if (!sdl2_audio_init())
    {
        SDL_Log("Audio init failed; continuing without sound.");
    }

    fe->running = 1;
    *out_fe = fe;
    return 1;

fail:
    sdl2_frontend_destroy(fe);
    return 0;
}

int sdl2_frontend_pump(Sdl2Frontend* fe)
{
    if (!fe) return 0;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT) fe->running = 0;
        if (e.type == SDL_KEYDOWN)
        {
            if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) fe->running = 0;
            if (e.key.keysym.scancode == SDL_SCANCODE_F11) sdl2_frontend_toggle_fullscreen(fe);
        }
    }

    // Merge keyboard + gamepad → controller 0
    nes_set_controller_state(0, build_pad0_from_inputs(fe->gc));
    return fe->running;
}

void sdl2_frontend_present(Sdl2Frontend* fe)
{
    if (!fe) return;

    int pitch_bytes = 0;
    const uint32_t* fb = nes_framebuffer_argb8888(&pitch_bytes);
    if (fb) SDL_UpdateTexture(fe->tex, NULL, fb, pitch_bytes);

    SDL_RenderClear(fe->ren);
    SDL_RenderCopy(fe->ren, fe->tex, NULL, NULL);
    SDL_RenderPresent(fe->ren);
}

void sdl2_frontend_toggle_fullscreen(Sdl2Frontend* fe)
{
    if (!fe) return;
    Uint32 flags = SDL_GetWindowFlags(fe->win);
    int fs = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : 1;
    SDL_SetWindowFullscreen(fe->win, fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void sdl2_frontend_destroy(Sdl2Frontend* fe)
{
    sdl2_audio_shutdown();
    if (!fe) { SDL_Quit(); return; }
    if (fe->gc)  { SDL_GameControllerClose(fe->gc); fe->gc = NULL; }
    if (fe->tex) SDL_DestroyTexture(fe->tex);
    if (fe->ren) SDL_DestroyRenderer(fe->ren);
    if (fe->win) SDL_DestroyWindow(fe->win);
    free(fe);
    SDL_Quit();
}

/* --- Optional conveniences ------------------------------------------------ */
void sdl2_frontend_set_window_title(Sdl2Frontend* fe, const char* title)
{
    if (!fe || !title) return;
    SDL_SetWindowTitle(fe->win, title);
}

void sdl2_frontend_set_integer_scale(Sdl2Frontend* fe, int enabled)
{
    if (!fe) return;
    fe->integer_scale = enabled ? 1 : 0;
    SDL_RenderSetIntegerScale(fe->ren, fe->integer_scale ? SDL_TRUE : SDL_FALSE);
}

void sdl2_frontend_get_draw_size(Sdl2Frontend* fe, int* out_w, int* out_h)
{
    if (!fe) return;
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(fe->ren, &w, &h);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}
