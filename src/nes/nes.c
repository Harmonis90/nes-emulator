#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "mapper.h"
#include "cartridge.h"
#include "ines.h"
// #include "apu.h"


#define NES_CPU_FREQ_HZ 1789773u
#define NTSC_FPS 60u
#define CPU_CYCLES_PER_FRAME ((uint64_t)(NES_CPU_FREQ_HZ / NTSC_FPS)) // 29829
#define WATCHDOG_MULTIPLIER 10u
#define WATCHDOG_BUDGET (CPU_CYCLES_PER_FRAME * WATCHDOG_MULTIPLIER)

static uint64_t s_frame_counter = 0;

// --- Helpers ---------------------------------------------------------------
static inline void ppu_advance_by_cpu_delta(uint64_t c0, uint64_t c1)
{
    uint64_t delta_cpu = c1 - c0;
    if (delta_cpu)
    {
        ppu_step((int)delta_cpu);
    }
}

// Step exactly one CPU instruction and advance PPU accordingly.
static inline void step_one_instruction_and_tick_ppu(void)
{
    uint64_t c0 = cpu_get_cycles();
    cpu_step();
    uint64_t c1 = cpu_get_cycles();
    ppu_advance_by_cpu_delta(c0, c1);
}

static inline int watchdog_tripped(uint64_t start)
{
    return (cpu_get_cycles() - start) > WATCHDOG_BUDGET;
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
    bus_reset();
    ppu_reset();
    cpu_reset();
}

// Run until we see the NEXT vblank rising edge from the current point.
// That is: sync to a vblank edge, then run a whole frame until the next edge.
uint64_t nes_step_frame(void)
{
    uint64_t target = ppu_frame_count() + 1;
    while (ppu_frame_count() < target) {
        step_one_instruction_and_tick_ppu();
    }
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
        step_one_instruction_and_tick_ppu();
    }
}

uint64_t nes_frame_count(void)
{
    return s_frame_counter;
}

void nes_shutdown(void)
{
    // Placeholder for future resource cleanup (e.g., SDL shutdown).
    // Nothing required for pure core.
}