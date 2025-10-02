#pragma once
#include <stdint.h>

typedef struct {
    // TODO: LFSR, mode, envelope, timer, length...
    int dummy;
} apu_noise_t;

void  apu_noise_reset(apu_noise_t* n);
void  apu_noise_write(apu_noise_t* n, uint16_t reg, uint8_t v); // $400C–$400F
void  apu_noise_step_timer(apu_noise_t* n, int cpu_cycles);
float apu_noise_output(const apu_noise_t* n); // [-1..1]
