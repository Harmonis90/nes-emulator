#include "audio/apu_dmc.h"

void apu_dmc_reset(apu_dmc_t* d) { (void)d; }
void apu_dmc_write(apu_dmc_t* d, uint16_t reg, uint8_t v) { (void)d; (void)reg; (void)v; }
void apu_dmc_step_timer(apu_dmc_t* d, int cpu_cycles) { (void)d; (void)cpu_cycles; }
float apu_dmc_output(const apu_dmc_t* d) { (void)d; return 0.0f; }
