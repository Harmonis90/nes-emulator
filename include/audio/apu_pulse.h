#pragma once
#include <stdint.h>

// NES APU Pulse channel (used for Pulse 1 and Pulse 2)
// Implements: duty sequencer, envelope (with loop/constant), sweep, timer, length counter.

typedef struct {
    // --- configuration ---
    uint8_t is_ch1;      // 1 for pulse1 ($4000–$4003), 0 for pulse2 ($4004–$4007)
    uint8_t enabled;     // from $4015 bit (set via apu_pulse_set_enabled)

    // --- registers (latched) ---
    // $4000/$4004: DD L C VVVV
    //   DD   = duty (bits 6–7)
    //   L    = length counter halt (and envelope loop)
    //   C    = constant volume (if 1) else envelope
    //   VVVV = volume or envelope period
    uint8_t reg_4000;

    // $4001/$4005: E PPP N SSS
    //   E    = sweep enable
    //   PPP  = sweep period (divider; bits 4–6)
    //   N    = negate
    //   SSS  = shift
    uint8_t reg_4001;

    // $4002/$4006: timer low
    uint8_t reg_4002;

    // $4003/$4007: LLLLL HHH
    //   LLLLL = length index (bits 3–7)
    //   HHH   = timer high (bits 0–2)
    uint8_t reg_4003;

    // --- derived / dynamic state ---
    // timer period (11-bit)
    uint16_t timer;          // current reload period
    int32_t  timer_cnt;      // countdown (APU clocks)
    uint8_t  seq_step;       // 0..7 duty sequencer position

    // envelope unit
    uint8_t envelope_start;  // when set, reloads divider and volume from period
    uint8_t envelope_div;    // divider counter
    uint8_t envelope_vol;    // current 0..15
    // cached bits
    uint8_t duty;            // 0..3
    uint8_t len_halt;        // length counter halt & envelope loop
    uint8_t const_vol;       // use constant volume
    uint8_t vol_period;      // 0..15 (period or constant volume)

    // length counter
    uint8_t length;          // 0..(max length)

    // sweep unit
    uint8_t sweep_enable;
    uint8_t sweep_period;    // 0..7 (reg bits 4..6)
    uint8_t sweep_negate;    // 0/1
    uint8_t sweep_shift;     // 0..7
    uint8_t sweep_div;       // divider counter
    uint8_t sweep_reload;    // reload flag

    // mute from invalid period / sweep target
    uint8_t mute_sweep;      // set if target period invalid
} apu_pulse_t;

// Initialize as Pulse 1 (is_ch1=1) or Pulse 2 (is_ch1=0)
void apu_pulse_init(apu_pulse_t* p, int is_ch1);

// Reset dynamic state (called by init and APU reset)
void apu_pulse_reset(apu_pulse_t* p);

// Enable/disable via $4015; disabling clears length counter per hardware
void apu_pulse_set_enabled(apu_pulse_t* p, int enabled);

// Per-register write ($4000–$4003 for ch1, $4004–$4007 for ch2)
void apu_pulse_write(apu_pulse_t* p, uint16_t addr, uint8_t v);

// Timer advance in *CPU cycles*
void apu_pulse_step_timer(apu_pulse_t* p, int cpu_cycles);

// Frame sequencer clocks
// Quarter-frame clocks the envelope; half-frame clocks length & sweep.
void apu_pulse_clock_quarter(apu_pulse_t* p);
void apu_pulse_clock_half(apu_pulse_t* p);

// Mono sample in [-1, 1] (very lightweight; duty -> 0/1 with envelope amplitude)
float apu_pulse_output(const apu_pulse_t* p);

// Helper for $4015 read (length > 0)
static inline int apu_pulse_length_nonzero(const apu_pulse_t* p) { return p->length != 0; }
