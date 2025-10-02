//
// Created by Seth on 9/24/2025.
//

#ifndef SDL2_FRONTEND_H
#define SDL2_FRONTEND_H
#include <stdint.h>


#ifdef __cplusplus
extern "C"{
#endif


/* ----------------------------------------------------------------------------
Configuration passed at creation time
------------------------------------------------------------------------- */
typedef struct Sdl2Config
{
    const char* title; /* Window title (nullable → "NES")              */
    int scale; /* Initial window scale multiplier (default 3)  */
    int vsync; /* 0 = off, 1 = on (renderer present vsync)     */
    int integer_scale; /* 0 = free scale, 1 = integer pixel scale */
}Sdl2Config;

/* Opaque frontend handle (do not inspect fields). */
typedef struct Sdl2Frontend Sdl2Frontend;

/* ----------------------------------------------------------------------------
Lifecycle
------------------------------------------------------------------------- */
/* Create the SDL2 frontend (window, renderer, texture).
Returns non-zero on success, zero on failure.
On success, *out_fe receives a valid handle you must destroy with
sdl2_frontend_destroy().

Notes:
- This will initialize SDL with VIDEO | GAMECONTROLLER | AUDIO.
- The frontend expects the core to supply a persistent 256x240 ARGB8888
framebuffer via:  const uint32_t* nes_framebuffer_argb8888(int* pitch_bytes);
- Controller input is read from keyboard each pump and forwarded to the core via:
void nes_set_controller_state(int pad_index, uint8_t state);
(Mapping: Z=A, X=B, A=Select, S=Start, arrows=D-Pad)
*/
int sdl2_frontend_create(const Sdl2Config* cfg, Sdl2Frontend** out_fe);

/* Poll SDL events and push current input state into the core.
Returns non-zero while the app should continue running, zero to quit.
Hotkeys handled internally:
- ESC: quit
- F11: toggle fullscreen desktop
*/
int sdl2_frontend_pump(Sdl2Frontend* fe);

/* Upload the current core framebuffer and present the frame. */
void sdl2_frontend_present(Sdl2Frontend* fe);

/* Toggle fullscreen desktop mode. */
void sdl2_frontend_toggle_fullscreen(Sdl2Frontend* fe);

/* Destroy the frontend and quit SDL subsystems initialized by create(). */
void sdl2_frontend_destroy(Sdl2Frontend* fe);

/* ----------------------------------------------------------------------------
Optional convenience (no-ops if the implementation doesn't support them).
These are kept small and stable for future growth.
------------------------------------------------------------------------- */

/* Change window title at runtime (null → no change). */
void sdl2_frontend_set_window_title(Sdl2Frontend* fe, const char* title);

/* Enable/disable integer scaling at runtime (0/1). */
void sdl2_frontend_set_integer_scale(Sdl2Frontend* fe, int enabled);

/* Query drawable (logical) size after scaling (outputs may be NULL). */
void sdl2_frontend_get_draw_size(Sdl2Frontend* fe, int* out_w, int* out_h);



#ifdef __cplusplus
}
#endif
#endif //SDL2_FRONTEND_H
