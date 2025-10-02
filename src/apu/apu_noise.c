#include "audio/apu_noise.h"

void apu_noise_reset(apu_noise_t* n) { (void)n; }
void apu_noise_write(apu_noise_t* n, uint16_t reg, uint8_t v) { (void)n; (void)reg; (void)v; }
void apu_noise_step_timer(apu_noise_t* n, int cpu_cycles) { (void)n; (void)cpu_cycles; }
float apu_noise_output(const apu_noise_t* n) { (void)n; return 0.0f; }
