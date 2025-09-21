// 6502 addressing & fetch helpers shared by opcode implementations.
// These functions do not perform cycle accounting; opcodes add cycles themselves.

#ifndef NES_CPU_ADDR_H
#define NES_CPU_ADDR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

// Effective address + page-cross flag (used by modes that may add a cycle).
typedef struct
{
    uint16_t addr; // effective 16-bit address
    uint8_t crossed; // 1 if high byte changed during indexing, else 0
}eff_t;

// -----------------------------------------------------------------------------
// Instruction byte fetch (advances PC)
// -----------------------------------------------------------------------------
uint8_t cpu_fetch8(void); // read byte at PC, PC++
uint16_t cpu_fetch16(void); // little-endian 16-bit at PC, PC+=2

// -----------------------------------------------------------------------------
// Addressing modes that return a plain 16-bit address
// -----------------------------------------------------------------------------
uint16_t cpu_addr_imm(void); // Immediate: returns address of operand (PC), then increments PC
uint16_t cpu_addr_zp(void); // Zero Page: $00nn
uint16_t cpu_addr_zpx(void); // Zero Page,X: ($00nn + X) & 0xFF
uint16_t cpu_addr_zpy(void); // Zero Page,Y: ($00nn + Y) & 0xFF
uint16_t cpu_addr_abs(void); // Absolute: $nnnn
uint16_t cpu_addr_inx(void); // (Indirect,X): read ZP ptr at ($00nn + X), wrap within ZP
uint16_t cpu_addr_ind(void); // (Indirect): JMP ($nnnn) with 6502 page-wrap bug

// -----------------------------------------------------------------------------
// Addressing modes that may cross a page boundary (and thus cost +1 cycle):
// They return eff_t with .crossed = 1 if page crossed, else 0.
// -----------------------------------------------------------------------------
eff_t cpu_addr_abx(void); // Absolute,X
eff_t cpu_addr_aby(void); // Absolute,Y
eff_t cpu_addr_iny(void); // (Indirect),Y: read ZP ptr at $00nn (wrap), then +Y

#ifdef __cplusplus
}
#endif


#endif //NES_CPU_ADDR_H
