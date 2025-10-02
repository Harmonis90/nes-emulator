#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "mapper.h"
#include "cartridge.h"
#include "ines.h"
#include "nes.h"
#include "controller.h"
#include "apu.h"

#include "debug_checks.h"

#define NES_CPU_FREQ_HZ 1789773u
#define NTSC_FPS 60u
#define CPU_CYCLES_PER_FRAME ((uint64_t)(NES_CPU_FREQ_HZ / NTSC_FPS)) // 29829
#define WATCHDOG_MULTIPLIER 10u
#define WATCHDOG_BUDGET (CPU_CYCLES_PER_FRAME * WATCHDOG_MULTIPLIER)

static uint64_t s_frame_counter = 0;

static uint32_t g_fb[NES_W * NES_H];
static int g_test_frame = 0;

// --- Helpers ---------------------------------------------------------------


// Step exactly one CPU instruction and advance PPU accordingly.
static inline void step_one_instruction_and_tick_all(void)
{
    DBG_WRAP_STEP(cpu_step());
    // uint64_t c0 = cpu_get_cycles();
    // cpu_step();
    // uint64_t c1 = cpu_get_cycles();
    // uint64_t delta_cpu = c1 - c0;
    // if (delta_cpu)
    // {
    //     ppu_step((int)delta_cpu);
    //     apu_step((int)delta_cpu);
    // }
}

static inline int watchdog_tripped(uint64_t start)
{
    return (cpu_get_cycles() - start) > WATCHDOG_BUDGET;
}

static void draw_test_pattern(uint32_t* fb, int frame)
{
    for (int y = 0; y < NES_H; y++)
    {
        for (int x = 0; x < NES_W; x++)
        {
            unsigned bar = (unsigned)((x * 8) / NES_W);
            uint32_t c =
                bar==0?0xFF202020: bar==1?0xFFFF0000: bar==2?0xFF00FF00: bar==3?0xFF0000FF:
                bar==4?0xFFFFFF00: bar==5?0xFFFF00FF: bar==6?0xFF00FFFF: 0xFFFFFFFF;
            if (y == (frame % NES_H)) c ^= 0x00FFFFFF;
            fb[y * NES_W + x] = c;
        }
    }
}
// Force a ~440 Hz square wave (middle A) on Pulse 1.
// Call once after apu_reset() and after SDL audio init.
void apu_debug_beep_440(void)
{
    // Enable Pulse 1
    apu_write(0x4015, 0x01);

    // $4000: duty=50% (2), length halt=0, constant volume=1, volume=10
    // bits: DD L C VVVV
    //   DD=2 -> 50% duty
    //   L=0  -> no loop
    //   C=1  -> constant volume
    //   VVVV=10 -> volume=10
    apu_write(0x4000, (uint8_t)((2u << 6) | (1u << 4) | 10u));

    // Period = (CPU / (16 * freq)) - 1
    // For 440 Hz on NTSC (1,789,773 Hz): ≈ 253
    uint16_t period = 253;

    apu_write(0x4002, (uint8_t)(period & 0xFF));     // low 8 bits
    apu_write(0x4003, (uint8_t)((period >> 8) & 0x07)); // high 3 bits + length idx=0

}


// -------- public API -------------------------------------------------------
void nes_init(void)
{
    bus_reset();
    ppu_reset();
    cpu_reset();
    s_frame_counter = 0;
}

void nes_reset(void)
{
    apu_reset();
    apu_set_region(APU_MODE_NTSC);

    apu_debug_beep_440();
    bus_reset();
    ppu_reset();
    cpu_reset();

}

// Run until we see the NEXT vblank rising edge from the current point.
// That is: sync to a vblank edge, then run a whole frame until the next edge.
uint64_t nes_step_frame(void)
{
    // 1) Wait for the very next *rising* edge (entering vblank)
    bool prev = ppu_in_vblank();
    uint64_t guard = cpu_get_cycles() + 60000; // ~2 CPU frames
    while (ppu_in_vblank() == prev) {
        step_one_instruction_and_tick_all();
        if (cpu_get_cycles() > guard) goto bailout;
    }

    // 2) We're now IN vblank. Run until vblank falls and rises again → next frame.
    bool saw_clear = false;
    guard = cpu_get_cycles() + 180000; // ~6 CPU frames
    for (;;) {
        bool v = ppu_in_vblank();
        if (!v) saw_clear = true;
        if (saw_clear && v) break;           // back to *rising* edge → inside vblank
        step_one_instruction_and_tick_all();
        if (cpu_get_cycles() > guard) goto bailout;
    }
    return ++s_frame_counter;

    bailout:
        fprintf(stderr, "[WATCHDOG] nes_step_frame bailed; vblank=%d\n", (int)ppu_in_vblank());
    return ++s_frame_counter;
}



// Run N frames. Returns the new frame counter.
uint64_t nes_run_frames(uint32_t count)
{
    while (count--) nes_step_frame();
    return s_frame_counter;
}

// Run for approximately `seconds` of emulated time using CPU cycle budget.
void nes_step_seconds(double seconds)
{
    if (seconds <= 0.0) return;
    uint64_t start = cpu_get_cycles();
    uint64_t budget = (uint64_t)(seconds * (double)NES_CPU_FREQ_HZ);

    while ((cpu_get_cycles() - start) < budget)
    {
        step_one_instruction_and_tick_all();
    }
}

uint64_t nes_frame_count(void)
{
    return s_frame_counter;
}

const uint32_t* nes_framebuffer_argb8888(int* out_pitch_bytes)
{
    if (out_pitch_bytes) *out_pitch_bytes = NES_W * 4;
    /* Render the current PPU state into our persistent buffer */
    static uint32_t g_fb[NES_W * NES_H];
    ppu_render_argb8888(g_fb, NES_W * 4);
    return g_fb;
}

void nes_set_controller_state(int pad_index, uint8_t state)
{
    if (pad_index < 0) pad_index = 0;
    if (pad_index > 1) pad_index = 1;
    controller_set_state(pad_index, state);
}

void nes_shutdown(void)
{
    // Placeholder for future resource cleanup (e.g., SDL shutdown).
    // Nothing required for pure core.
}


