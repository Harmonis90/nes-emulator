// NES APU (Audio Processing Unit) public interface.
// - CPU register space: $4000–$4017
// - Timing: advance via apu_step(cpu_cycles)
// - Audio: pull mixed mono samples via apu_read_samples(), or set a sink callback
//
// Typical usage:
//   apu_reset();
//   apu_set_region(APU_MODE_NTSC);
//   apu_set_sample_rate(48000);
//
//   // In your CPU loop:
//   apu_step(cpu_cycles_elapsed);
//
//   // In your audio thread/callback:
//   int16_t buf[1024];
//   size_t n = apu_read_samples(buf, 1024);  // mono int16 frames
//
// Optionally:
//   apu_set_sink(my_sink, my_user_ptr); // APU will push frames when ready

#ifndef NES_APU_H
#define NES_APU_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

// ---- Reset / configuration ----
void apu_reset(void);

// NTSC vs PAL timing differences (frame sequencer rate/dividers)
typedef enum
{
    APU_MODE_NTSC = 0,
    APU_MODE_PAL = 1,
}apu_region_t;

void apu_set_region(apu_region_t region);

// Output sample rate in Hz (e.g., 44100 or 48000).
// Call once before running; can be changed between runs if needed.
void apu_set_sample_rate(uint32_t sample_rate_hz);

// Optional: select 4-step (default) or 5-step frame sequencer behavior.
// (Affects envelope/length/timer clocking cadence; $4017 bit 7 in hardware.)
void apu_set_sequencer_5step(int enable);

// ---- CPU <-> APU register interface ($4000–$4017) ----
uint8_t apu_read(uint16_t addr);  // Reads at $4015/$4016/$4017 return meaningful bits; others typically open-bus
void apu_write(uint16_t addr, uint8_t v); // Writes to $4000–$4017 control channels + DMC

// ---- Timing ----
// Advance the APU by N CPU cycles (CPU ~1.789773 MHz NTSC, ~1.662607 MHz PAL).
// Call this alongside your CPU stepping so the sequencer and channel timers run in lockstep.
void apu_step(int cpu_cycles);

// ---- Audio output (mono) ----
//
// The APU mixes pulse1, pulse2, triangle, noise, and DMC into a single mono stream.
// You can either PULL from an internal ring buffer or set a PUSH sink callback.
// Both can be used simultaneously; the sink receives the same frames that go into the ring.

// Pull model: returns number of frames copied to 'out' (int16 mono).
size_t apu_read_samples(int16_t* out, size_t max_frames);

// How many frames are currently buffered and ready to read.
size_t apu_frames_available(void);

// Push model: set a sink callback (optional). The callback should be fast and non-blocking.
typedef void (*apu_sink_cb)(const uint16_t* samples, size_t frames, void* user);
void apu_set_sink(apu_sink_cb cb, void* user);

// ---- Misc / testing aids ----
// Hard-mute/unmute individual channels for debugging (1 = mute).
void apu_debug_mute_pulse1(int mute);
void apu_debug_mute_pulse2(int mute);
void apu_debug_mute_triangle(int mute);
void apu_debug_mute_noise(int mute);
void apu_debug_mute_dmc(int mute);



#ifdef __cplusplus
}
#endif
#endif //NES_APU_H
