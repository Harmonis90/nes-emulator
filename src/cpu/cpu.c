//
// Created by Seth on 8/30/2025.
//
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "bus.h"

// 6502 cpu state
typedef struct
{
    uint8_t A; // accumulator
    uint8_t X; // x register
    uint8_t Y; // y register
    uint8_t SP; // stack pointer
    uint16_t PC; // program counter
    uint8_t P; // status flags
    uint8_t halted;
} CPU;

static CPU cpu;

// ---- flag helpers ----
static inline uint8_t get_flag(uint8_t f) { return (cpu.P & f) ? 1 : 0;}
static inline void set_flag(uint8_t f, int on) { if (on) cpu.P |= f; else cpu.P &= (uint8_t)~f; }
static inline void set_zn(uint8_t v) { set_flag(FLAG_Z, v==0); set_flag(FLAG_N, v & 0x80); }

// ----- stack helpers (stack lives at $0100-$01FF) -----
static inline void push8(uint8_t v) { cpu_write((uint16_t)(0x0100 | cpu.SP), v); cpu.SP--;}
static inline void push16(uint16_t v) { push8((uint8_t)(v >> 8) & 0xFF); push8((uint8_t)(v & 0xFF)); }
static inline uint8_t pop8(void) { cpu.SP++; return cpu_read((uint16_t)(0x0100 | cpu.SP)); }
static inline uint16_t pop16(void) { uint8_t lo = pop8(); uint8_t hi = pop8(); return (uint16_t)((hi << 8) | lo); }

// ---- fetch helpers ----
static inline uint8_t fetch8(void) { uint8_t v = cpu_read(cpu.PC); cpu.PC = (uint16_t)cpu.PC + 1; return v;}
static inline uint16_t fetch16(void) { uint8_t lo = fetch8(); uint8_t hi = fetch8(); return (uint16_t) (hi << 8) | lo;}

// ---- addressing helpers ----
static inline uint16_t addr_imm(void) { return cpu.PC++; }
static inline uint16_t addr_zp(void) { return (uint16_t)fetch8(); }
static inline uint16_t addr_zpx(void) { return (uint16_t)((fetch8() + cpu.X) & 0xFF); }
static inline uint16_t addr_zpy(void) { return (uint16_t)((fetch8() + cpu.Y) & 0xFF); }
static inline uint16_t addr_abs(void) { return fetch16(); }
static inline uint16_t addr_abx(void) { uint16_t a = fetch16(); return (uint16_t)(a + cpu.X); }
static inline uint16_t addr_aby(void) { uint16_t a = fetch16(); return (uint16_t)(a + cpu.Y); }

// Relative addressing: signed 8-bit offset added to the *next* PC.
static inline uint16_t addr_rel(void)
{
    int8_t off = (int8_t)fetch8();
    return (uint16_t)(cpu.PC + (int8_t)off);
}

// Zero-page 16-bit read with wraparound (hi from (zp+1)&0xFF)
static inline uint16_t zp_ptr(uint8_t zp_addr)
{
    uint8_t lo = cpu_read((uint16_t)zp_addr);
    uint8_t hi = cpu_read((uint16_t)((uint8_t)(zp_addr + 1)));
    return (uint16_t)((hi << 8) | lo);
}

// (zp,X) — indexed-indirect
static inline uint16_t addr_inx(void)
{
    uint8_t zp_addr = (uint8_t)(fetch8() + cpu.X); // wrap in zero-page
    return zp_ptr(zp_addr);
}

// (zp),Y — indirect-indexed
static inline uint16_t addr_iny(void)
{
    uint8_t zp_addr = fetch8();
    uint16_t base = zp_ptr(zp_addr);
    return (uint16_t)(base + cpu.Y);
}

static inline uint16_t addr_ind(void){
    uint16_t ptr = fetch16();  // FF 80 -> 0x80FF
    uint8_t lo = cpu_read(ptr);
    uint16_t hi_addr = (uint16_t)((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    uint8_t hi = cpu_read(hi_addr);
    uint16_t target = (uint16_t)((hi << 8) | lo);
    // printf("DBG addr_ind: ptr=%04X lo@%04X=%02X hi@%04X=%02X -> %04X\n",
    //        ptr, ptr, lo, hi_addr, hi, target);
    return target;
}

// ---- lifecycle ----
void cpu_reset(void)
{
    memset(&cpu, 0, sizeof(CPU));
    cpu.SP = 0xFD; // typical reset value
    cpu.P = FLAG_I | FLAG_U; // IRQ disabled, unused flag set
    // Read reset vector at $FFFC/$FFFD
    uint16_t lo = cpu_read(VEC_RESET);
    uint16_t hi = cpu_read((uint16_t)VEC_RESET + 1);
    cpu.PC = (uint16_t)((hi << 8) | lo);
}

// ----- arithmetic core (NES ignores decimal mode) -----
static inline uint8_t getC(void) { return get_flag(FLAG_C); }
static inline void do_adc(uint8_t m)
{
    uint16_t sum = (uint16_t)cpu.A + m + (uint16_t)get_flag(FLAG_C);
    uint8_t result = (uint8_t)(sum & 0xFF);
    set_flag(FLAG_C, (sum > 0xFF));

    // Overflow: if sign of A == sign of m and sign of result != sign of A
    set_flag(FLAG_V, (~(cpu.A ^ m) & (cpu.A ^ result) & 0x80) != 0);
    cpu.A = result; set_zn(cpu.A);
}

static inline void do_sub(uint8_t m)
{
    // SBC = A + (~m) + C
    do_adc((uint8_t)(m ^ 0xFF));
}

// ----- ops (subset, enough to run small test programs) -----
static inline void op_NOP(void) {/* do nothing */}

static inline void op_BRK(void)
{
    // Minimal: push PC+1 and P|B|U, set I, jump to IRQ/BRK vector, and halt for our toy runner
    uint16_t ret = (uint16_t)cpu.PC; // BRK is 1-byte; PC already at next byte
    push16(ret);
    push8((uint8_t)(cpu.P | FLAG_B | FLAG_U));
    set_flag(FLAG_I, 1);
    uint8_t lo = cpu_read(VEC_IRQ_BRK);
    uint8_t hi = cpu_read((uint16_t)(VEC_IRQ_BRK + 1));
    cpu.PC = (uint16_t)((hi << 8) | lo);
    cpu.halted = 1;
    printf("BRK at %04X -> handler %04X\n", (uint16_t)(ret - 1), cpu.PC);
}

static inline void op_SEI(void){ set_flag(FLAG_I, 1); }
static inline void op_CLI(void) { set_flag(FLAG_I, 0); }
static inline void op_CLD(void) { set_flag(FLAG_D, 0); }

// Loads (A)
static inline void op_LDA_imm(void) { cpu.A = fetch8(); set_zn(cpu.A); }
static inline void op_LDA_zp(void) { cpu.A = cpu_read(addr_zp()); set_zn(cpu.A); }
static inline void op_LDA_abs(void) { cpu.A = cpu_read(addr_abs()); set_zn(cpu.A); }
static inline void op_LDA_zpx(void) { cpu.A = cpu_read(addr_zpx()); set_zn(cpu.A); }
static inline void op_LDA_abx(void) { cpu.A = cpu_read(addr_abx()); set_zn(cpu.A); }
static inline void op_LDA_aby(void) { cpu.A = cpu_read(addr_aby()); set_zn(cpu.A); }

static inline void op_LDA_inx(void) { cpu.A = cpu_read(addr_inx()); set_zn(cpu.A); }
static inline void op_LDA_iny(void) { cpu.A = cpu_read(addr_iny()); set_zn(cpu.A); }


// Loads (X)
static inline void op_LDX_imm(void) { cpu.X = fetch8(); set_zn(cpu.X); }
static inline void op_LDX_zp(void) { cpu.X = cpu_read(addr_zp()); set_zn(cpu.X); }
static inline void op_LDX_zpy(void) { cpu.X = cpu_read(addr_zpy()); set_zn(cpu.X); }
static inline void op_LDX_abs(void) { cpu.X = cpu_read(addr_abs()); set_zn(cpu.X); }
static inline void op_LDX_aby(void) { cpu.X = cpu_read(addr_aby()); set_zn(cpu.X); }

// Loads (Y)
static inline void op_LDY_imm(void) {cpu.Y = fetch8(); set_zn(cpu.Y); }
static inline void op_LDY_zp(void) { cpu.Y = cpu_read(addr_zp()); set_zn(cpu.Y); }
static inline void op_LDY_zpx(void) { cpu.Y = cpu_read(addr_zpx()); set_zn(cpu.Y); }
static inline void op_LDY_abs(void) { cpu.Y = cpu_read(addr_abs()); set_zn(cpu.Y); }
static inline void op_LDY_abx(void) { cpu.Y = cpu_read(addr_abx()); set_zn(cpu.Y); }

// Stores
static inline void op_STA_zp(void) { cpu_write(addr_zp(), cpu.A); }
static inline void op_STA_zpx(void) { cpu_write(addr_zpx(), cpu.A); }
static inline void op_STA_abs(void) { cpu_write(addr_abs(), cpu.A); }
static inline void op_STA_abx(void) { cpu_write(addr_abx(), cpu.A); }
static inline void op_STA_aby(void) { cpu_write(addr_aby(), cpu.A); }

static inline void op_STA_inx(void) { cpu_write(addr_inx(), cpu.A); }
static inline void op_STA_iny(void) { cpu_write(addr_iny(), cpu.A); }

// STX
static inline void op_STX_zp(void) { cpu_write(addr_zp(), cpu.X); }
static inline void op_STX_zpy(void) { cpu_write(addr_zpy(), cpu.X); }
static inline void op_STX_abs(void) { cpu_write(addr_abs(), cpu.X); }

// STY
static inline void op_STY_zp(void) { cpu_write(addr_zp(), cpu.Y); }
static inline void op_STY_zpx(void) { cpu_write(addr_zpx(), cpu.Y); }
static inline void op_STY_abs(void) { cpu_write(addr_abs(), cpu.Y); }

// TYA (Y -> A, sets Z/N)
static inline void op_TYA(void) { cpu.A = cpu.Y; set_zn(cpu.A); }

// Transfers / Inc/Dec
static inline void op_TAX(void) { cpu.X = cpu.A; set_zn(cpu.X); }
static inline void op_TXA(void) { cpu.A = cpu.X; set_zn(cpu.A); }
static inline void op_TAY(void) { cpu.Y = cpu.A; set_zn(cpu.Y); }
static inline void op_INX(void) { cpu.X = (uint8_t)(cpu.X + 1); set_zn(cpu.X); }
static inline void op_DEX(void) {cpu.X = (uint8_t)(cpu.X - 1); set_zn(cpu.X); }

// Arithmetic
// ADC
static inline void op_ADC_imm(void) { do_adc(cpu_read(addr_imm())); }
static inline void op_ADC_zp(void) { do_adc(cpu_read(addr_zp())); }
static inline void op_ADC_zpx(void) { do_adc(cpu_read(addr_zpx())); }
static inline void op_ADC_abs(void) { do_adc(cpu_read(addr_abs())); }
static inline void op_ADC_abx(void) { do_adc(cpu_read(addr_abx())); }
static inline void op_ADC_aby(void) { do_adc(cpu_read(addr_aby())); }
static inline void op_ADC_inx(void) { do_adc(cpu_read(addr_inx())); }
static inline void op_ADC_iny(void) { do_adc(cpu_read(addr_iny())); }

// SBC
static inline void op_SBC_imm(void) { do_sub(cpu_read(addr_imm())); }
static inline void op_SBC_zp(void) { do_sub(cpu_read(addr_zp())); }
static inline void op_SBC_zpx(void) { do_sub(cpu_read(addr_zpx())); }
static inline void op_SBC_abs(void) { do_sub(cpu_read(addr_abs())); }
static inline void op_SBC_abx(void) { do_sub(cpu_read(addr_abx())); }
static inline void op_SBC_aby(void) { do_sub(cpu_read(addr_aby())); }
static inline void op_SBC_inx(void) { do_sub(cpu_read(addr_inx())); }
static inline void op_SBC_iny(void) { do_sub(cpu_read(addr_iny())); }

// ---------- LOGICAL OPS (update Z/N from A) ----------
static inline void op_AND_imm(void) { cpu.A &= fetch8(); set_zn(cpu.A); }
static inline void op_AND_zp(void) { cpu.A &= cpu_read(addr_zp()); set_zn(cpu.A); }
static inline void op_AND_zpx(void) { cpu.A &= cpu_read(addr_zpx()); set_zn(cpu.A); }
static inline void op_AND_abs(void) { cpu.A &= cpu_read(addr_abs()); set_zn(cpu.A); }
static inline void op_AND_abx(void) { cpu.A &= cpu_read(addr_abx()); set_zn(cpu.A); }
static inline void op_AND_aby(void) { cpu.A &= cpu_read(addr_aby()); set_zn(cpu.A); }
static inline void op_AND_inx(void) { cpu.A &= cpu_read(addr_inx()); set_zn(cpu.A); }
static inline void op_AND_iny(void) { cpu.A &= cpu_read(addr_iny()); set_zn(cpu.A); }

static inline void op_ORA_imm(void) { cpu.A |= fetch8(); set_zn(cpu.A); }
static inline void op_ORA_zp(void) { cpu.A |= cpu_read(addr_zp()); set_zn(cpu.A); }
static inline void op_ORA_zpx(void) { cpu.A |= cpu_read(addr_zpx()); set_zn(cpu.A); }
static inline void op_ORA_abs(void) { cpu.A |= cpu_read(addr_abs()); set_zn(cpu.A); }
static inline void op_ORA_abx(void) { cpu.A |= cpu_read(addr_abx()); set_zn(cpu.A); }
static inline void op_ORA_aby(void) { cpu.A |= cpu_read(addr_aby()); set_zn(cpu.A); }
static inline void op_ORA_inx(void) { cpu.A |= cpu_read(addr_inx()); set_zn(cpu.A); }
static inline void op_ORA_iny(void) { cpu.A |= cpu_read(addr_iny()); set_zn(cpu.A); }

static inline void op_EOR_imm(void) { cpu.A ^= fetch8(); set_zn(cpu.A); }
static inline void op_EOR_zp(void) { cpu.A ^= cpu_read(addr_zp()); set_zn(cpu.A); }
static inline void op_EOR_zpx(void) { cpu.A ^= cpu_read(addr_zpx()); set_zn(cpu.A); }
static inline void op_EOR_abs(void) { cpu.A ^= cpu_read(addr_abs()); set_zn(cpu.A); }
static inline void op_EOR_abx(void) { cpu.A ^= cpu_read(addr_abx()); set_zn(cpu.A); }
static inline void op_EOR_aby(void) { cpu.A ^= cpu_read(addr_aby()); set_zn(cpu.A); }
static inline void op_EOR_inx(void) { cpu.A ^= cpu_read(addr_inx()); set_zn(cpu.A); }
static inline void op_EOR_iny(void) { cpu.A ^= cpu_read(addr_iny()); set_zn(cpu.A); }

// --------- COMPARES (set Z/N/C; do NOT change A/X/Y) ----------
static inline void do_cmp(uint8_t reg, uint8_t m)
{
    uint16_t diff = (uint16_t)reg - (uint16_t)m;
    set_flag(FLAG_C, reg >= m);
    set_flag(FLAG_Z, ((uint8_t)diff) == 0);
    set_flag(FLAG_N, (diff & 0x80) != 0);
}

// CMP (A vs M)
static inline void op_CMP_imm(void) { do_cmp(cpu.A, fetch8()); }
static inline void op_CMP_zp(void) { do_cmp(cpu.A, cpu_read(addr_zp())); }
static inline void op_CMP_zpx(void) { do_cmp(cpu.A, cpu_read(addr_zpx())); }
static inline void op_CMP_abs(void) { do_cmp(cpu.A, cpu_read(addr_abs())); }
static inline void op_CMP_abx(void) { do_cmp(cpu.A, cpu_read(addr_abx())); }
static inline void op_CMP_aby(void) { do_cmp(cpu.A, cpu_read(addr_aby())); }
static inline void op_CMP_inx(void) { do_cmp(cpu.A, cpu_read(addr_inx())); }
static inline void op_CMP_iny(void) { do_cmp(cpu.A, cpu_read(addr_iny())); }

// CPX (X vs M)
static inline void op_CPX_imm(void) { do_cmp(cpu.X, fetch8()); }
static inline void op_CPX_zp(void) { do_cmp(cpu.X, cpu_read(addr_zp())); }
static inline void op_CPX_abs(void) { do_cmp(cpu.X, cpu_read(addr_abs())); }

// CPY (Y vs M)
static inline void op_CPY_imm(void) { do_cmp(cpu.Y, fetch8()); }
static inline void op_CPY_zp(void) { do_cmp(cpu.Y, cpu_read(addr_zp())); }
static inline void op_CPY_abs(void) { do_cmp(cpu.Y, cpu_read(addr_abs())); }

// --------- BIT (test A & M; set Z from A&M, N from M7, V from M6) ----------
static inline void op_BIT_zp(void)
{
    uint8_t m = cpu_read(addr_zp());
    set_flag(FLAG_Z, (cpu.A & m) == 0);
    set_flag(FLAG_V, (m & 0x40) != 0);
    set_flag(FLAG_N, (m & 0x80) != 0);
}

static inline void op_BIT_abs(void)
{
    uint8_t m = cpu_read(addr_abs());
    set_flag(FLAG_Z, (cpu.A & m) == 0);
    set_flag(FLAG_V, (m & 0x40) != 0);
    set_flag(FLAG_N, (m & 0x80) != 0);
}
// ---------- SHIFTS & ROTATES ----------
// ASL: shift left, old bit7 -> C
static inline uint8_t do_asl(uint8_t v)
{
    set_flag(FLAG_C, v & 0x80);
    v <<= 1;
    set_zn(v);
    return v;
}

// LSR: shift right, old bit0 -> C
static inline uint8_t do_lsr(uint8_t v)
{
    set_flag(FLAG_C, v & 0x01);
    v >>= 1;
    set_zn(v);
    return v;
}

// ROL: rotate left through C
static inline uint8_t do_rol(uint8_t v)
{
    uint8_t c = get_flag(FLAG_C);
    set_flag(FLAG_C, v & 0x80);
    v = (uint8_t)((v << 1) | c);
    set_zn(v);
    return v;
}

// ROR: rotate right through C
static inline uint8_t do_ror(uint8_t v)
{
    uint8_t c = get_flag(FLAG_C);
    set_flag(FLAG_C, v & 0x01);
    v = (uint8_t)((v >> 1) | (c << 7));
    set_zn(v);
    return v;
}

// ASL
static inline void op_ASL_A(void) { cpu.A = do_asl(cpu.A); }
static inline void op_ASL_zp(void) { uint16_t a = addr_zp(); cpu_write(a, do_asl(cpu_read(a))); }
static inline void op_ASL_zpx(void) { uint16_t a = addr_zpx(); cpu_write(a, do_asl(cpu_read(a))); }
static inline void op_ASL_abs(void) { uint16_t a = addr_abs(); cpu_write(a, do_asl(cpu_read(a))); }
static inline void op_ASL_abx(void) { uint16_t a = addr_abx(); cpu_write(a, do_asl(cpu_read(a))); }

// LSR
static inline void op_LSR_A(void) { cpu.A = do_lsr(cpu.A); }
static inline void op_LSR_zp(void) { uint16_t a = addr_zp(); cpu_write(a, do_lsr(cpu_read(a))); }
static inline void op_LSR_zpx(void) { uint16_t a = addr_zpx(); cpu_write(a, do_lsr(cpu_read(a))); }
static inline void op_LSR_abs(void) { uint16_t a = addr_abs(); cpu_write(a, do_lsr(cpu_read(a))); }
static inline void op_LSR_abx(void) { uint16_t a = addr_abx(); cpu_write(a, do_lsr(cpu_read(a))); }

// ROL
static inline void op_ROL_A(void) { cpu.A = do_rol(cpu.A); }
static inline void op_ROL_zp(void) { uint16_t a = addr_zp(); cpu_write(a, do_rol(cpu_read(a))); }
static inline void op_ROL_zpx(void) { uint16_t a = addr_zpx(); cpu_write(a, do_rol(cpu_read(a))); }
static inline void op_ROL_abs(void) { uint16_t a = addr_abs(); cpu_write(a, do_rol(cpu_read(a))); }
static inline void op_ROL_abx(void) { uint16_t a = addr_abx(); cpu_write(a, do_rol(cpu_read(a))); }

// ROR
static inline void op_ROR_A(void) { cpu.A = do_ror(cpu.A); }
static inline void op_ROR_zp(void) { uint16_t a = addr_zp(); cpu_write(a, do_ror(cpu_read(a))); }
static inline void op_ROR_zpx(void) { uint16_t a = addr_zpx(); cpu_write(a, do_ror(cpu_read(a))); }
static inline void op_ROR_abs(void) { uint16_t a = addr_abs(); cpu_write(a, do_ror(cpu_read(a)));}
static inline void op_ROR_abx(void) { uint16_t a = addr_abx(); cpu_write(a, do_ror(cpu_read(a)));}

// ---------- INC/DEC (memory), INY/DEY, and flag ops ----------
// Memory INC: set Z/N from result; C unaffected (6502 behavior)
static inline uint8_t do_inc(uint8_t v) { v = (uint8_t)(v + 1); set_zn(v); return v; }
static inline uint8_t do_dec(uint8_t v) { v= (uint8_t)(v - 1); set_zn(v); return v; }

static inline void op_INC_zp(void) { uint16_t a = addr_zp(); cpu_write(a, do_inc(cpu_read(a))); }
static inline void op_INC_zpx(void) { uint16_t a = addr_zpx(); cpu_write(a, do_inc(cpu_read(a))); }
static inline void op_INC_abs(void) { uint16_t a = addr_abs(); cpu_write(a, do_inc(cpu_read(a))); }
static inline void op_INC_abx(void) { uint16_t a = addr_abx(); cpu_write(a, do_inc(cpu_read(a))); }

static inline void op_DEC_zp(void) { uint16_t a = addr_zp(); cpu_write(a, do_dec(cpu_read(a))); }
static inline void op_DEC_zpx(void) { uint16_t a = addr_zpx(); cpu_write(a, do_dec(cpu_read(a))); }
static inline void op_DEC_abs(void) { uint16_t a = addr_abs(); cpu_write(a, do_dec(cpu_read(a))); }
static inline void op_DEC_abx(void) { uint16_t a = addr_abx(); cpu_write(a, do_dec(cpu_read(a))); }

// Register INC/DEC for Y
static inline void op_INY(void) { cpu.Y = (uint8_t)(cpu.Y + 1); set_zn(cpu.Y); }
static inline void op_DEY(void) { cpu.Y = (uint8_t)(cpu.Y - 1); set_zn(cpu.Y); }

// Flag ops
static inline void op_CLC(void) { set_flag(FLAG_C, 0); }
static inline void op_SEC(void) { set_flag(FLAG_C, 1); }
static inline void op_CLV(void) { set_flag(FLAG_V, 0); }

// ---------- STACK & TRANSFERS ----------
// Note: your push8/pop8 helpers already target $0100|SP
//Push A
static inline void op_PHA(void){ push8(cpu.A); }

// Pull A (updates Z/N)
static inline void op_PLA(void)
{
    cpu.A = pop8();
    set_zn(cpu.A);
}

// Push P (B and U bits set when pushing)
static inline void op_PHP(void)
{
    uint8_t v = (uint8_t)(cpu.P | FLAG_B | FLAG_U);
    push8(v);
}

// Pull P (bit 5 U forced set; bit 4 B cleared as per 6502 quirk)
static inline void op_PLP(void)
{
    uint8_t v = pop8();
    v |= FLAG_U;
    v &= (uint8_t)~FLAG_B;
    cpu.P = v;
}

// Transfer Stack Pointer <-> X
static inline void op_TSX(void) { cpu.X = cpu.SP; set_zn(cpu.X); }
static inline void op_TXS(void) { cpu.SP = cpu.X; }

// ===== Interrupt entry helper =====
// Jumps to vector `vec`, pushing PC and P. If `set_break` is 1, sets B in the pushed copy.
static inline void interrupt_enter(uint16_t vec, int set_break)
{
    uint16_t ret = cpu.PC;
    // Push return address (PC) and status
    push16(ret);
    uint8_t p = cpu.P;
    if (set_break) p |= FLAG_B; else p &= (uint8_t)~FLAG_B;
    p |= FLAG_U;
    push8(p);
    set_flag(FLAG_I, 1);
    // Load vector
    uint8_t lo = cpu_read(vec);
    uint8_t hi = cpu_read((uint16_t)(vec + 1));
    cpu.PC = (uint16_t)((hi << 8) | lo);

}

// ===== RTI (Return from Interrupt) =====
static inline void op_RTI(void)
{
    // Pull P, then PC (lo, hi). U bit forced set; B is whatever was in P on stack.
    uint8_t p = pop8();
    p |= FLAG_U;
    cpu.P = p;
    uint8_t lo = pop8();
    uint8_t hi = pop8();
    cpu.PC = (uint16_t)((hi << 8) | lo);
}


// Control flow
static inline void op_JMP_abs(void) { cpu.PC = addr_abs(); }
static inline void op_JSR_abs(void)
{
    uint16_t target = addr_abs();
    // push return address (PC-1 per 6502 behavior)
    uint16_t ret = (uint16_t)(cpu.PC - 1);
    push16(ret);
    cpu.PC = target;

}

static inline void op_JMP_ind(void) { cpu.PC = addr_ind(); }

static inline void op_RTS(void)
{
    uint16_t ret = pop16();
    cpu.PC = (uint16_t)(ret + 1);
}

// ---- Branches (relative) ----
// Note: we ignore cycle penalties (page-cross) for now.
static inline void branch_if(int cond)
{
    uint16_t target = addr_rel();
    if (cond) cpu.PC = target;
}

static inline void op_BEQ(void) { branch_if(get_flag(FLAG_Z)); }
static inline void op_BNE(void) { branch_if(!get_flag(FLAG_Z)); }
static inline void op_BMI(void) { branch_if(get_flag(FLAG_N)); }
static inline void op_BPL(void) { branch_if(!get_flag(FLAG_N)); }
static inline void op_BCS(void) { branch_if(get_flag(FLAG_C)); }
static inline void op_BCC(void) { branch_if(!get_flag(FLAG_C)); }
static inline void op_BVS(void) { branch_if(get_flag(FLAG_V)); }
static inline void op_BVC(void) { branch_if(!get_flag(FLAG_V)); }


// ----- dispatcher -----
void cpu_step(void)
{
    if (cpu.halted) return;

    uint8_t op = fetch8();

    switch (op)
    {
        // System / flags
        case 0xEA: op_NOP(); break;
        case 0x00: op_BRK(); break;
        case 0x78: op_SEI(); break;
        case 0x58: op_CLI(); break;
        case 0xD8: op_CLD(); break;

        // Loads (A)
        case 0xA9: op_LDA_imm(); break;
        case 0xA5: op_LDA_zp(); break;
        case 0xB5: op_LDA_zpx(); break;
        case 0xAD: op_LDA_abs(); break;
        case 0xBD: op_LDA_abx(); break;
        case 0xB9: op_LDA_aby(); break;
        case 0xA1: op_LDA_inx(); break;
        case 0xB1: op_LDA_iny(); break;

        // Loads (X)
        case 0xA2: op_LDX_imm(); break;
        case 0xA6: op_LDX_zp(); break;
        case 0xB6: op_LDX_zpy(); break;
        case 0xAE: op_LDX_abs(); break;
        case 0xBE: op_LDX_aby(); break;

        // Loads (Y)
        case 0xA0: op_LDY_imm(); break;
        case 0xA4: op_LDY_zp(); break;
        case 0xB4: op_LDY_zpx(); break;
        case 0xAC: op_LDY_abs(); break;
        case 0xBC: op_LDY_abx(); break;

        // Stores
        case 0x85: op_STA_zp();  break;
        case 0x95: op_STA_zpx(); break;
        case 0x8D: op_STA_abs(); break;
        case 0x9D: op_STA_abx(); break;
        case 0x99: op_STA_aby(); break;
        case 0x81: op_STA_inx(); break;
        case 0x91: op_STA_iny(); break;

        // STX
        case 0x86: op_STX_zp(); break;
        case 0x96: op_STX_zpy(); break;
        case 0x8E: op_STX_abs(); break;

        // STY
        case 0x84: op_STY_zp(); break;
        case 0x94: op_STY_zpx(); break;
        case 0x8C: op_STY_abs(); break;

        // TYA
        case 0x98: op_TYA(); break;


        // Transfers / Inc/Dec
        case 0xAA: op_TAX(); break;
        case 0x8A: op_TXA(); break;
        case 0xA8: op_TAY(); break;
        case 0xE8: op_INX(); break;
        case 0xCA: op_DEX(); break;

        // Arithmetic
        // ADC
        case 0x69: op_ADC_imm(); break;
        case 0x65: op_ADC_zp(); break;
        case 0x75: op_ADC_zpx(); break;
        case 0x6D: op_ADC_abs(); break;
        case 0x7D: op_ADC_abx(); break;
        case 0x79: op_ADC_aby(); break;
        case 0x61: op_ADC_inx(); break;
        case 0x71: op_ADC_iny(); break;

        // SBC
        case 0xE9: op_SBC_imm(); break;
        case 0xE5: op_SBC_zp(); break;
        case 0xF5: op_SBC_zpx(); break;
        case 0xED: op_SBC_abs(); break;
        case 0xFD: op_SBC_abx(); break;
        case 0xF9: op_SBC_aby(); break;
        case 0xE1: op_SBC_inx(); break;
        case 0xF1: op_SBC_iny(); break;

        // Logic (AND)
        case 0x29: op_AND_imm(); break;
        case 0x25: op_AND_zp(); break;
        case 0x35: op_AND_zpx(); break;
        case 0x2D: op_AND_abs(); break;
        case 0x3D: op_AND_abx(); break;
        case 0x39: op_AND_aby(); break;
        case 0x21: op_AND_inx(); break;
        case 0x31: op_AND_iny(); break;

        // Logic (OR)
        case 0x09: op_ORA_imm(); break;
        case 0x05: op_ORA_zp(); break;
        case 0x15: op_ORA_zpx(); break;
        case 0x0D: op_ORA_abs(); break;
        case 0x1D: op_ORA_abx(); break;
        case 0x19: op_ORA_aby(); break;
        case 0x01: op_ORA_inx(); break;
        case 0x11: op_ORA_iny(); break;

        // Logic (XOR)
        case 0x49: op_EOR_imm(); break;
        case 0x45: op_EOR_zp(); break;
        case 0x55: op_EOR_zpx(); break;
        case 0x4D: op_EOR_abs(); break;
        case 0x5D: op_EOR_abx(); break;
        case 0x59: op_EOR_aby(); break;
        case 0x41: op_EOR_inx(); break;
        case 0x51: op_EOR_iny(); break;

        // CMP (A vs M)
        case 0xC9: op_CMP_imm(); break;
        case 0xC5: op_CMP_zp(); break;
        case 0xD5: op_CMP_zpx(); break;
        case 0xCD: op_CMP_abs(); break;
        case 0xDD: op_CMP_abx(); break;
        case 0xD9: op_CMP_aby(); break;
        case 0xC1: op_CMP_inx(); break;
        case 0xD1: op_CMP_iny(); break;

        // CPX (X vs M)
        case 0xE0: op_CPX_imm(); break;
        case 0xE4: op_CPX_zp(); break;
        case 0xEC: op_CPX_abs(); break;

        // CPY (Y vs M)
        case 0xC0: op_CPY_imm(); break;
        case 0xC4: op_CPY_zp(); break;
        case 0xCC: op_CPY_abs(); break;

        // BIT (A & M -> Z ; M7->N ; M6->V)
        case 0x24: op_BIT_zp(); break;
        case 0x2C: op_BIT_abs(); break;

        // ASL
        case 0x0A: op_ASL_A(); break;
        case 0x06: op_ASL_zp(); break;
        case 0x16: op_ASL_zpx(); break;
        case 0x0E: op_ASL_abs(); break;
        case 0x1E: op_ASL_abx(); break;

        // LSR
        case 0x4A: op_LSR_A(); break;
        case 0x46: op_LSR_zp(); break;
        case 0x56: op_LSR_zpx(); break;
        case 0x4E: op_LSR_abs(); break;
        case 0x5E: op_LSR_abx(); break;


        // ROL
        case 0x2A: op_ROL_A(); break;
        case 0x26: op_ROL_zp(); break;
        case 0x36: op_ROL_zpx(); break;
        case 0x2E: op_ROL_abs(); break;
        case 0x3E: op_ROL_abx(); break;


        // ROR
        case 0x6A: op_ROR_A(); break;
        case 0x66: op_ROR_zp(); break;
        case 0x76: op_ROR_zpx(); break;
        case 0x6E: op_ROR_abs(); break;
        case 0x7E: op_ROR_abx(); break;

        // INC (memory)
        case 0xE6: op_INC_zp(); break;
        case 0xF6: op_INC_zpx(); break;
        case 0xEE: op_INC_abs(); break;
        case 0xFE: op_INC_abx(); break;

        // DEC (memory)
        case 0xC6: op_DEC_zp(); break;
        case 0xD6: op_DEC_zpx(); break;
        case 0xCE: op_DEC_abs(); break;
        case 0xDE: op_DEC_abx(); break;

        // INY / DEY
        case 0xC8: op_INY(); break;
        case 0x88: op_DEY(); break;

        // CLC / SEC / CLV
        case 0x18: op_CLC(); break;
        case 0x38: op_SEC(); break;
        case 0xB8: op_CLV(); break;

        // Stack & transfers
        case 0x48: op_PHA(); break;
        case 0x68: op_PLA(); break;
        case 0x08: op_PHP(); break;
        case 0x28: op_PLP(); break;
        case 0xBA: op_TSX(); break;
        case 0x9A: op_TXS(); break;

        // RTI
        case 0x40: op_RTI(); break;

        // Control flow
        case 0x4C: op_JMP_abs(); break;
        case 0x20: op_JSR_abs(); break;
        case 0x6C: op_JMP_ind(); break;
        case 0x60: op_RTS(); break;

        // Branches (relative)
        case 0xF0: op_BEQ(); break;
        case 0xD0: op_BNE(); break;
        case 0x30: op_BMI(); break;
        case 0x10: op_BPL(); break;
        case 0xB0: op_BCS(); break;
        case 0x90: op_BCC(); break;
        case 0x70: op_BVS(); break;
        case 0x50: op_BVC(); break;

        default:
            printf("Unimplemented opcode: %02X at %04X\n", op, (uint16_t)(cpu.PC - 1));
            break;
    }
}

// --- interrupts ---
void cpu_irq(void)
{
    // Only enter IRQ if I flag is clear
    if (!(cpu.P & FLAG_I))
    {
        interrupt_enter(VEC_IRQ_BRK, 0); // Break flag clear for hardware IRQ
    }
}
void cpu_nmi(void)
{
    // NMI always taken (ignores I)
    interrupt_enter(VEC_NMI, 0);        // Break flag clear for NMI
}

// --- debug/getters/setters ---
uint16_t cpu_get_pc(void) { return cpu.PC; }
uint8_t  cpu_get_sp(void) { return cpu.SP; }
uint8_t  cpu_get_p(void)  { return cpu.P;  }
uint8_t  cpu_get_a(void)  { return cpu.A;  }
uint8_t  cpu_get_x(void)  { return cpu.X;  }
uint8_t  cpu_get_y(void)  { return cpu.Y;  }

void cpu_set_pc(uint16_t pc) { cpu.PC = pc; }
void cpu_set_p(uint8_t p) {cpu.P = p; }