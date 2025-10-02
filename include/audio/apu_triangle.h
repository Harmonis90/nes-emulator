#pragma once
#include <stdint.h>

typedef struct {
    // TODO: linear counter, length, timer, sequencer index...
    int dummy;
} apu_triangle_t;

void  apu_triangle_reset(apu_triangle_t* t);
void  apu_triangle_write(apu_triangle_t* t, uint16_t reg, uint8_t v); // $4008–$400B
void  apu_triangle_step_timer(apu_triangle_t* t, int cpu_cycles);
float apu_triangle_output(const apu_triangle_t* t); // [-1..1]
