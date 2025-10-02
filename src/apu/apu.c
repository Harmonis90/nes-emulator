// src/apu/apu.c
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "apu.h"
#include "audio/apu_pulse.h"
#include "audio/apu_triangle.h"
#include "audio/apu_noise.h"
#include "audio/apu_dmc.h"
#include "audio/apu_mixer.h"

// ------------------------------
// Timing constants
// ------------------------------
#define NTSC_CPU_HZ 1789773u
#define PAL_CPU_HZ  1662607u

// 4-step sequence (NTSC) markers (CPU cycles from start of frame)
#define NTSC_4STEP_0  3729u
#define NTSC_4STEP_1  7457u
#define NTSC_4STEP_2 11186u
#define NTSC_4STEP_3 14916u   // end; frame IRQ if enabled

// Approximate total for 5-step (no IRQ at end)
#define NTSC_5STEP_END 18641u

// ------------------------------
// Helpers
// ------------------------------
static inline int16_t float_to_i16(float x) {
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    float y = x * 32767.0f;
    if (y > 32767.0f) y = 32767.0f;
    if (y < -32768.0f) y = -32768.0f;
    return (int16_t)y;
}

// ------------------------------
// Minimal per-channel flags used for $4015 readback
// ------------------------------
typedef struct {
    uint8_t enabled;
    uint8_t length_nonzero;
} ch_stub_t;

// ------------------------------
// Global APU state
// ------------------------------
static struct {
    // Config
    apu_region_t region;
    uint32_t cpu_hz;
    uint32_t sample_rate;

    // Frame sequencer
    uint8_t  five_step;     // $4017 bit 7
    uint8_t  irq_inhibit;   // $4017 bit 6
    uint8_t  frame_irq;     // sticky until read $4015
    uint32_t seq_cycle;     // cycles within current frame
    uint32_t seq_end_ntsc;  // end-of-frame cycle count

    // Channel submodules
    apu_pulse_t    pulse1_impl;
    apu_pulse_t    pulse2_impl;
    apu_triangle_t tri_impl;
    apu_noise_t    noise_impl;
    apu_dmc_t      dmc_impl;

    // Public status bits for $4015 (mirrors submodules where needed)
    ch_stub_t pulse1, pulse2, triangle, noise, dmc;

    // Registers latch ($4000–$4017)
    uint8_t regs[0x18];

    // Resampling
    double cycles_per_sample;
    double cycle_accum;

    // Output ring buffer (mono int16)
    #define APU_RING_CAP 8192u  // must be power of two
    int16_t ring[APU_RING_CAP];
    volatile uint32_t r_head;   // write index
    volatile uint32_t r_tail;   // read index

    // Optional sink callback
    apu_sink_cb sink;
    void*       sink_user;

    // Debug mutes
    uint8_t mute_p1, mute_p2, mute_tri, mute_noise, mute_dmc;
} g;

// ------------------------------
// Ring buffer
// ------------------------------
static inline uint32_t ring_count(void) {
    return (g.r_head - g.r_tail) & (APU_RING_CAP - 1);
}
static inline uint32_t ring_space(void) {
    return (APU_RING_CAP - 1) - ring_count();
}
static inline void ring_push(int16_t s) {
    if (ring_space() == 0) return; // drop if full
    g.ring[g.r_head] = s;
    g.r_head = (g.r_head + 1) & (APU_RING_CAP - 1);
}
static inline uint32_t ring_pop(int16_t* out, uint32_t maxn) {
    uint32_t n = 0;
    while (n < maxn && g.r_tail != g.r_head) {
        out[n++] = g.ring[g.r_tail];
        g.r_tail = (g.r_tail + 1) & (APU_RING_CAP - 1);
    }
    return n;
}

// ------------------------------
// Timing setup
// ------------------------------
static void recompute_timing(void) {
    g.cpu_hz = (g.region == APU_MODE_PAL) ? PAL_CPU_HZ : NTSC_CPU_HZ;
    if (g.sample_rate == 0) g.sample_rate = 48000;
    g.cycles_per_sample = (double)g.cpu_hz / (double)g.sample_rate;
    g.seq_end_ntsc = g.five_step ? NTSC_5STEP_END : NTSC_4STEP_3;
}

// ------------------------------
// Mixer hook: pull floats from channels, apply mutes, mix to int16
// ------------------------------
static int16_t mix_sample(void) {
    float p1 = g.mute_p1 ? 0.0f : apu_pulse_output(&g.pulse1_impl);
    float p2 = g.mute_p2 ? 0.0f : apu_pulse_output(&g.pulse2_impl);
    float tr = g.mute_tri ? 0.0f : apu_triangle_output(&g.tri_impl);
    float no = g.mute_noise ? 0.0f : apu_noise_output(&g.noise_impl);
    float dm = g.mute_dmc ? 0.0f : apu_dmc_output(&g.dmc_impl);

    float mixed = apu_mixer_mix(p1, p2, tr, no, dm);
    return float_to_i16(mixed);
}

// ------------------------------
// Frame sequencer boundaries (NTSC 4-step markers)
// We'll tick quarter/half clocks when crossing these marks.
// ------------------------------
static void frame_sequencer_tick(uint32_t before, uint32_t after) {
    const uint32_t marks[4] = {NTSC_4STEP_0, NTSC_4STEP_1, NTSC_4STEP_2, NTSC_4STEP_3};
    for (int i = 0; i < 4; ++i) {
        if (before < marks[i] && after >= marks[i]) {
            // Quarter frame: envelope + linear counter (triangle) would clock
            apu_pulse_clock_quarter(&g.pulse1_impl);
            apu_pulse_clock_quarter(&g.pulse2_impl);
            // TODO: triangle envelope/linear counter when implemented
            // TODO: noise envelope when implemented

            // Half frame at steps 1 and 3: length + sweep
            if (i == 1 || i == 3) {
                apu_pulse_clock_half(&g.pulse1_impl);
                apu_pulse_clock_half(&g.pulse2_impl);
                // TODO: triangle length
                // TODO: noise length
            }

            // End of 4-step raises frame IRQ unless inhibited (5-step has no IRQ here)
            if (!g.five_step && i == 3 && !g.irq_inhibit) {
                g.frame_irq = 1;
            }
        }
    }
}

// ------------------------------
// Public API
// ------------------------------
void apu_reset(void) {
    memset(&g, 0, sizeof(g));
    g.region = APU_MODE_NTSC;
    g.sample_rate = 48000;
    recompute_timing();

    // init submodules
    apu_pulse_init(&g.pulse1_impl, 1); // Pulse 1
    apu_pulse_init(&g.pulse2_impl, 0); // Pulse 2
    apu_pulse_reset(&g.pulse1_impl);
    apu_pulse_reset(&g.pulse2_impl);

    apu_triangle_reset(&g.tri_impl);
    apu_noise_reset(&g.noise_impl);
    apu_dmc_reset(&g.dmc_impl);

    // channels disabled by default
    g.pulse1.enabled = g.pulse2.enabled = 0;
    g.triangle.enabled = g.noise.enabled = g.dmc.enabled = 0;
}

void apu_set_region(apu_region_t region) {
    g.region = region;
    recompute_timing();
}

void apu_set_sample_rate(uint32_t rate_hz) {
    g.sample_rate = (rate_hz ? rate_hz : 48000);
    recompute_timing();
}

void apu_set_sequencer_5step(int enable) {
    g.five_step = (enable != 0);
    recompute_timing();
}

uint8_t apu_read(uint16_t addr) {
    if (addr == 0x4015) {
        // Update length_nonzero mirrors from submodules (for accuracy).
        g.pulse1.length_nonzero = (uint8_t)apu_pulse_length_nonzero(&g.pulse1_impl);
        g.pulse2.length_nonzero = (uint8_t)apu_pulse_length_nonzero(&g.pulse2_impl);
        // TODO: set triangle/noise/dmc length mirrors once implemented

        uint8_t v = 0;
        // bit 6: frame IRQ
        if (g.frame_irq) v |= 0x40;
        // bit 7: DMC IRQ (not implemented here)

        // length bits
        if (g.dmc.length_nonzero)      v |= (1u << 4);
        if (g.noise.length_nonzero)    v |= (1u << 3);
        if (g.triangle.length_nonzero) v |= (1u << 2);
        if (g.pulse2.length_nonzero)   v |= (1u << 1);
        if (g.pulse1.length_nonzero)   v |= (1u << 0);

        // reading $4015 clears frame IRQ
        g.frame_irq = 0;
        return v;
    }
    // Other reads here are typically open-bus; controllers are $4016/$4017 handled elsewhere.
    return 0x00;
}

// Route $4000–$4013 to channels; $4015/$4017 to status/frame control.
void apu_write(uint16_t addr, uint8_t v) {
    if (addr < 0x4000 || addr > 0x4017) return;
    g.regs[addr - 0x4000] = v;

    // Pulse 1: $4000–$4003
    if (addr <= 0x4003) {
        apu_pulse_write(&g.pulse1_impl, addr, v);
        return;
    }
    // Pulse 2: $4004–$4007
    if (addr >= 0x4004 && addr <= 0x4007) {
        apu_pulse_write(&g.pulse2_impl, addr, v);
        return;
    }
    // Triangle: $4008–$400B
    if (addr >= 0x4008 && addr <= 0x400B) {
        apu_triangle_write(&g.tri_impl, addr, v);
        return;
    }
    // Noise: $400C–$400F
    if (addr >= 0x400C && addr <= 0x400F) {
        apu_noise_write(&g.noise_impl, addr, v);
        return;
    }
    // DMC: $4010–$4013
    if (addr >= 0x4010 && addr <= 0x4013) {
        apu_dmc_write(&g.dmc_impl, addr, v);
        return;
    }

    // $4015: channel enables
    if (addr == 0x4015) {
        g.pulse1.enabled   = (v & 0x01) != 0;
        g.pulse2.enabled   = (v & 0x02) != 0;
        g.triangle.enabled = (v & 0x04) != 0;
        g.noise.enabled    = (v & 0x08) != 0;
        g.dmc.enabled      = (v & 0x10) != 0;

        // Inform pulse modules (clears length when disabling)
        apu_pulse_set_enabled(&g.pulse1_impl, (v & 0x01) != 0);
        apu_pulse_set_enabled(&g.pulse2_impl, (v & 0x02) != 0);

        // For other channels, once implemented, mirror $4015 behavior (disable clears length)
        if (!(v & 0x04)) g.triangle.length_nonzero = 0;
        if (!(v & 0x08)) g.noise.length_nonzero = 0;
        if (!(v & 0x10)) g.dmc.length_nonzero = 0;

        return;
    }

    // $4017: frame counter
    if (addr == 0x4017) {
        g.five_step   = (v & 0x80) != 0;
        g.irq_inhibit = (v & 0x40) != 0;
        if (g.irq_inhibit) g.frame_irq = 0;
        // Writing $4017 resets the sequencer (and optionally clocks immediately; ignored here)
        g.seq_cycle = 0;
        recompute_timing();
        return;
    }

    // Otherwise ignored
}

void apu_step(int cpu_cycles) {
    if (cpu_cycles <= 0) return;

    // Frame sequencer crossings
    uint32_t before = g.seq_cycle;
    g.seq_cycle += (uint32_t)cpu_cycles;
    frame_sequencer_tick(before, g.seq_cycle);

    // Wrap at end of sequence
    uint32_t end = g.seq_end_ntsc; // TODO: PAL marks if needed
    if (g.seq_cycle >= end) {
        g.seq_cycle -= end;
    }

    // Advance channel timers (per-CPU-cycle resolution accepted by stubs)
    apu_pulse_step_timer(&g.pulse1_impl, cpu_cycles);
    apu_pulse_step_timer(&g.pulse2_impl, cpu_cycles);
    apu_triangle_step_timer(&g.tri_impl, cpu_cycles);
    apu_noise_step_timer(&g.noise_impl, cpu_cycles);
    apu_dmc_step_timer(&g.dmc_impl, cpu_cycles);

    // Produce output samples at configured rate
    g.cycle_accum += (double)cpu_cycles;
    while (g.cycle_accum >= g.cycles_per_sample) {
        g.cycle_accum -= g.cycles_per_sample;

        int16_t s = mix_sample();
        ring_push(s);

        if (g.sink) {
            g.sink(&s, 1, g.sink_user);
        }
    }
}

size_t apu_read_samples(int16_t* out, size_t max_frames) {
    if (!out || max_frames == 0) return 0;
    return (size_t)ring_pop(out, (uint32_t)max_frames);
}

size_t apu_frames_available(void) {
    return (size_t)ring_count();
}

void apu_set_sink(apu_sink_cb cb, void* user) {
    g.sink = cb;
    g.sink_user = user;
}

// Debug mutes
void apu_debug_mute_pulse1(int m)   { g.mute_p1   = (uint8_t)(m != 0); }
void apu_debug_mute_pulse2(int m)   { g.mute_p2   = (uint8_t)(m != 0); }
void apu_debug_mute_triangle(int m) { g.mute_tri  = (uint8_t)(m != 0); }
void apu_debug_mute_noise(int m)    { g.mute_noise= (uint8_t)(m != 0); }
void apu_debug_mute_dmc(int m)      { g.mute_dmc  = (uint8_t)(m != 0); }
