#include "audio/apu_triangle.h"

void apu_triangle_reset(apu_triangle_t* t) { (void)t; }
void apu_triangle_write(apu_triangle_t* t, uint16_t reg, uint8_t v) { (void)t; (void)reg; (void)v; }
void apu_triangle_step_timer(apu_triangle_t* t, int cpu_cycles) { (void)t; (void)cpu_cycles; }
float apu_triangle_output(const apu_triangle_t* t) { (void)t; return 0.0f; }
