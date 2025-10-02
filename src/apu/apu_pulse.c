#include <string.h>
#include "audio/apu_pulse.h"

// Length table per iNES spec (index in $4003/$4007 bits 3–7)
static const uint8_t LENGTH_TABLE[32] = {
    10, 254, 20,  2, 40,  4, 80,  6,
    160, 8,  60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192,24, 72, 26, 16, 28, 32, 30
};

// Duty sequences (8 steps)
static const uint8_t DUTY_SEQ[4][8] = {
    {0,1,0,0,0,0,0,0}, // 12.5%
    {0,1,1,0,0,0,0,0}, // 25%
    {0,1,1,1,1,0,0,0}, // 50%
    {1,0,0,1,1,1,1,1}  // 25% (negated)
};

static inline uint16_t period_from_regs(uint8_t lo, uint8_t hi3) {
    return (uint16_t)(((uint16_t)(hi3 & 0x07) << 8) | lo); // 11-bit
}

void apu_pulse_init(apu_pulse_t* p, int is_ch1) {
    memset(p, 0, sizeof(*p));
    p->is_ch1 = (uint8_t)(is_ch1 ? 1 : 0);
    // safe defaults
    p->timer = 0;
    p->timer_cnt = 0;
    p->seq_step = 0;
}

void apu_pulse_reset(apu_pulse_t* p) {
    uint8_t is_ch1 = p->is_ch1;
    memset(p, 0, sizeof(*p));
    p->is_ch1 = is_ch1;
}

void apu_pulse_set_enabled(apu_pulse_t* p, int enabled) {
    p->enabled = (uint8_t)(enabled != 0);
    if (!p->enabled) {
        p->length = 0; // disabling clears length counter
    }
}

// --- Envelope unit ---
static inline void envelope_reload(apu_pulse_t* p) {
    p->envelope_start = 0;
    p->envelope_div = p->vol_period;
    p->envelope_vol = 15;
}

void apu_pulse_write(apu_pulse_t* p, uint16_t addr, uint8_t v) {
    const uint16_t base = p->is_ch1 ? 0x4000u : 0x4004u;
    const uint16_t r = (uint16_t)(addr - base);

    switch (r) {
        case 0: // $4000/$4004
            p->reg_4000 = v;
            p->duty     = (uint8_t)((v >> 6) & 0x03);
            p->len_halt = (uint8_t)((v >> 5) & 0x01);
            p->const_vol= (uint8_t)((v >> 4) & 0x01);
            p->vol_period = (uint8_t)(v & 0x0F);
            // envelope restart happens when $4003/$4007 is written, not here
            break;
        case 1: // $4001/$4005
            p->reg_4001   = v;
            p->sweep_enable = (uint8_t)((v >> 7) & 1);
            p->sweep_period = (uint8_t)((v >> 4) & 0x07);
            p->sweep_negate = (uint8_t)((v >> 3) & 1);
            p->sweep_shift  = (uint8_t)(v & 0x07);
            p->sweep_reload = 1;
            break;
        case 2: // $4002/$4006 (timer low)
            p->reg_4002 = v;
            p->timer = period_from_regs(p->reg_4002, p->reg_4003);
            break;
        case 3: // $4003/$4007 (length + timer high)
            p->reg_4003 = v;
            // load length counter from table
            p->length = LENGTH_TABLE[(v >> 3) & 0x1F];
            // set timer high bits
            p->timer = period_from_regs(p->reg_4002, p->reg_4003);
            // reset duty sequencer to step 0
            p->seq_step = 0;
            // envelope restart
            p->envelope_start = 1;
            break;
        default:
            break;
    }
}

// Sweep target calculation and mute flag.
static inline uint16_t sweep_target(const apu_pulse_t* p, uint16_t t) {
    if (p->sweep_shift == 0) return t; // no change
    uint16_t change = (uint16_t)(t >> p->sweep_shift);
    if (p->sweep_negate) {
        // Pulse 1 does ones-complement extra -1
        // target = t - change - (is_ch1 ? 1 : 0)
        uint16_t sub = (uint16_t)(change + (p->is_ch1 ? 1 : 0));
        // Avoid underflow: if sub > t, target will wrap (invalid, will be muted)
        return (uint16_t)(t - sub);
    } else {
        return (uint16_t)(t + change);
    }
}

// Length counter clocked on half-frames if !len_halt and enabled
static inline void clock_length(apu_pulse_t* p) {
    if (!p->len_halt && p->length > 0) {
        p->length--;
    }
}

// Sweep clocked on half-frames
static inline void clock_sweep(apu_pulse_t* p) {
    // Mute if current period < 8 or > 0x7FF or target invalid
    p->mute_sweep = 0;
    if (p->timer < 8 || p->timer > 0x7FF) {
        p->mute_sweep = 1;
    }
    uint16_t tgt = sweep_target(p, p->timer);
    if (tgt > 0x7FF) {
        p->mute_sweep = 1;
    }

    if (p->sweep_div == 0) {
        if (p->sweep_enable && p->sweep_shift > 0 && !p->mute_sweep) {
            // Apply sweep: write back new period
            p->timer = tgt;
        }
        p->sweep_div = p->sweep_period;
    } else {
        p->sweep_div--;
    }

    if (p->sweep_reload) {
        p->sweep_reload = 0;
        p->sweep_div = p->sweep_period;
    }
}

// Envelope clocked on quarter-frames
static inline void clock_envelope(apu_pulse_t* p) {
    if (p->envelope_start) {
        envelope_reload(p);
    } else {
        if (p->envelope_div == 0) {
            p->envelope_div = p->vol_period;
            if (p->envelope_vol == 0) {
                if (p->len_halt) {
                    p->envelope_vol = 15;
                } // else stay at 0
            } else {
                p->envelope_vol--;
            }
        } else {
            p->envelope_div--;
        }
    }
}

void apu_pulse_clock_quarter(apu_pulse_t* p) {
    clock_envelope(p);
}

void apu_pulse_clock_half(apu_pulse_t* p) {
    clock_length(p);
    clock_sweep(p);
}

// Timer: advance sequencer with CPU-cycle resolution.
// Each time timer_cnt reaches zero, reload with (timer+1) and step the duty.
void apu_pulse_step_timer(apu_pulse_t* p, int cpu_cycles) {
    if (cpu_cycles <= 0) return;

    // If disabled or length is zero, we still tick timer to keep hardware-like behavior,
    // but output will be silenced by apu_pulse_output.
    p->timer_cnt -= cpu_cycles;
    while (p->timer_cnt <= 0) {
        int32_t reload = (int32_t)(p->timer + 1);
        // guard against zero reloads
        if (reload <= 0) reload = 1;
        p->timer_cnt += reload;
        p->seq_step = (uint8_t)((p->seq_step + 1) & 7);
    }
}

// Convert to float sample [-1,1].
// Silencing conditions: disabled, length==0, timer<8, sweep mute => output 0.
float apu_pulse_output(const apu_pulse_t* p) {
    if (!p->enabled) return 0.0f;
    if (p->length == 0) return 0.0f;
    if (p->timer < 8 || p->timer > 0x7FF) return 0.0f;
    if (p->mute_sweep) return 0.0f;

    // Duty bit
    uint8_t bit = DUTY_SEQ[p->duty][p->seq_step];

    // Volume from constant or envelope
    uint8_t vol = p->const_vol ? p->vol_period : p->envelope_vol;

    // Map to [-1,1] — NES mixer is nonlinear, but for channel output we keep it simple.
    float amp = (float)vol / 15.0f;
    float s = bit ? amp : 0.0f;

    // Center around ~0 by subtracting small DC (optional). Keep simple: leave as [0..amp].
    return s;
}
