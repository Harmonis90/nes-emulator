#include <stdint.h>

#include "cpu.h"
#include "bus.h"
#include "cpu_internal.h"

// -----------------------------------------------------------------------------
// CPU register state (private)
// -----------------------------------------------------------------------------
typedef struct
{
    uint8_t A; // Accumulator
    uint8_t X; // Index X
    uint8_t Y; // Index Y
    uint8_t P; // Status
    uint8_t SP; // Stack Pointer
    uint16_t PC; // Program Counter
}cpu_state_t;

static cpu_state_t s;

// -----------------------------------------------------------------------------
// Public accessors (cpu.h)
// -----------------------------------------------------------------------------
uint16_t cpu_get_pc(void) { return s.PC; }
void cpu_set_pc(uint16_t pc) { s.PC = pc; }

uint8_t cpu_get_sp(void) { return s.SP; }
void cpu_set_sp(uint8_t sp) { s.SP = sp; }

uint8_t cpu_get_p(void) { return s.P; }
void cpu_set_p(uint8_t p) { s.P = (uint8_t)(p | FLAG_U); }

uint8_t cpu_get_a(void) { return s.A; }
void cpu_set_a(uint8_t a) { s.A = a; }

uint8_t cpu_get_x(void) { return s.X; }
void cpu_set_x(uint8_t x) { s.X = x; }

uint8_t cpu_get_y(void) { return s.Y; }
void cpu_set_y(uint8_t y) { s.Y = y; }

// ----------------------------
// Instruction byte fetch
// ----------------------------
uint8_t fetch8(void)
{
    uint16_t pc = cpu_get_pc();
    uint8_t v = cpu_read(pc);
    cpu_set_pc((uint16_t)(pc + 1));
    return v;
}

uint16_t fetch16(void)
{
    uint8_t lo = fetch8();
    uint8_t hi = fetch8();
    return (uint16_t)((uint16_t)hi << 8 | lo);
}

// ----------------------------
// Stack operations
// ----------------------------
void push8(uint8_t v)
{
    uint8_t sp = cpu_get_sp();
    cpu_write((uint16_t)(0x0100 | sp), v);
    cpu_set_sp((uint8_t)(sp - 1));
}

void push16(uint16_t v)
{
    push8((uint8_t)(v >> 8));
    push8((uint8_t)(v & 0xFF));
}

uint8_t pop8(void)
{
    uint8_t sp = (uint8_t)(cpu_get_sp() + 1);
    cpu_set_sp(sp);
    return cpu_read((uint16_t)(0x0100 | sp));
}

uint16_t pop16(void)
{
    uint8_t lo = pop8();
    uint8_t hi = pop8();
    return (uint16_t)((uint16_t)hi << 8 | lo);
}

// ----------------------------
// Flag helpers
// ----------------------------
void set_flag(uint8_t flag_mask, int on)
{
    uint8_t p = cpu_get_p();
    if (on) p |= flag_mask;
    else p &= (uint8_t)~flag_mask;
    // Keep the unused bit set internally when writing P
    p |= FLAG_U;
    cpu_set_p(p);
}

uint8_t get_flag(uint8_t flag_mask)
{
    return (uint8_t)((cpu_get_p()) & flag_mask) ? 1 : 0;
}

void set_zn(uint8_t v)
{
    set_flag(FLAG_Z, v == 0);
    set_flag(FLAG_N, (v & 0x80) != 0);
}

// ----------------------------
// Interrupt enter helper
// ----------------------------
void interrupt_enter(uint16_t vec, int set_break)
{
    // Push return PC (points to next byte already for BRK; callers should ensure PC is correct)
    uint16_t ret = cpu_get_pc();
    push16(ret);

    // Push status with U set, and B optionally set/cleared for BRK vs IRQ/NMI
    uint8_t p = cpu_get_p();
    p |= FLAG_U;
    if (set_break) p |= FLAG_B;
    else p &= (uint8_t)~FLAG_B;
    push8(p);

    // Set I (mask IRQ) on entry
    set_flag(FLAG_I, 1);

    // Load vector
    uint8_t lo = cpu_read(vec);
    uint8_t hi = cpu_read((uint16_t)(vec + 1));
    cpu_set_pc((uint16_t)((uint16_t)hi << 8 | lo));
}