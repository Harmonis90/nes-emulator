#ifndef NES_CPU_TABLE_H
#define NES_CPU_TABLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

// ----------------------------------------------------------------------------
// Opcode handler type: every opcode implementation must be `void f(void)`
// (no args; it fetches operands, performs reads/writes, and calls cpu_cycles_add).
// ----------------------------------------------------------------------------
typedef void (*cpu_op_t)(void);

// ----------------------------------------------------------------------------
// Public tables (defined in src/cpu/cpu_table.c)
// ----------------------------------------------------------------------------

// 256-entry dispatch table: cpu_dispatch[opcode] is the function to execute.
extern const cpu_op_t cpu_dispatch[256];

// Baseline cycle count for each opcode (without conditional penalties).
// Branch +1 (taken) and +1 (page cross) and read-indexed +1 (page cross)
// are applied inside opcode implementations, not here.
extern const uint8_t cpu_base_cycles[256];

// Optional metadata for debugging / tracing / disassembly.
// - cpu_mnemonic[op]   -> "LDA", "STA", "JSR", ...
// - cpu_addrmode[op]   -> "#imm", "zp", "zp,X", "abs", "abs,X", "(ind),Y", etc.
extern const char* const cpu_mnemonic[256];
extern const char* const cpu_addrmode[256];

// Optional byte-length per opcode (1 = opcode only, 2 = +1 byte operand, 3 = +2 bytes).
// Useful for stepping/tracing when you don't execute the opcode.
extern const uint8_t cpu_instr_len[256];

#ifdef __cplusplus
}
#endif
#endif //NES_CPU_TABLE_H
