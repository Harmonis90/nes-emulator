//
// Created by Seth on 8/30/2025.
//

#ifndef NES_CPU_H
#define NES_CPU_H

#include <stdint.h>

// --- 6502 status flag bits ---
enum
{
    FLAG_C = 1 << 0, // Carry
    FLAG_Z = 1 << 1, // Zero
    FLAG_I = 1 << 2, // IRQ Disable
    FLAG_D = 1 << 3, // Decimal (unused in NES, but present)
    FLAG_B = 1 << 4, // Break
    FLAG_U = 1 << 5, // Unused (always set on pushes)
    FLAG_V = 1 << 6, // Overflow
    FLAG_N = 1 << 7, // Negative
};

void cpu_reset(void);
void cpu_step(void);

void cpu_irq(void);
void cpu_nmi(void);

// These operate on the internal CPU singleton in cpu.c.
uint16_t cpu_get_pc(void);
uint8_t cpu_get_sp(void);
uint8_t cpu_get_p(void);
uint8_t cpu_get_a(void);
uint8_t cpu_get_x(void);
uint8_t cpu_get_y(void);

void cpu_set_pc(uint16_t pc);
void cpu_set_p(uint8_t p);


#endif //NES_CPU_H
