#pragma once
#include <stdint.h>

typedef struct {
    // TODO: DMA address/length, sample buffer, output level, IRQ...
    int dummy;
} apu_dmc_t;

void  apu_dmc_reset(apu_dmc_t* d);
void  apu_dmc_write(apu_dmc_t* d, uint16_t reg, uint8_t v); // $4010–$4013
void  apu_dmc_step_timer(apu_dmc_t* d, int cpu_cycles);
float apu_dmc_output(const apu_dmc_t* d); // [-1..1] (approx; real DMC is stepped)
