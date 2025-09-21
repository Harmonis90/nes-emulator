#ifndef NES_CPU_H
#define NES_CPU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

// -----------------------------------------------------------------------------
// 6502 Status Flag Bits
// -----------------------------------------------------------------------------
enum
{
    FLAG_C = 1 << 0, // Carry
    FLAG_Z = 1 << 1, // Zero
    FLAG_I = 1 << 2, // IRQ Disable
    FLAG_D = 1 << 3, // Decimal (Unused on NES)
    FLAG_B = 1 << 4, // Break (only on pushes/pulls)
    FLAG_U = 1 << 5, // Unused (internal; keep set when storing P)
    FLAG_V = 1 << 6, // Overflow
    FLAG_N = 1 << 7, // Negative
};

// -----------------------------------------------------------------------------
// Lifecycle / Execution
// -----------------------------------------------------------------------------
void cpu_reset(void); // power-on/reset: reads reset vector, initializes registers
void cpu_step(void); // execute one opcode (including all implied memory cycles)

// Asynchronous interrupts (triggered by PPU/APU/mapper/etc.)
void cpu_irq(void); // maskable IRQ (respects I flag)
void cpu_nmi(void); // non-maskable interrupt

// -----------------------------------------------------------------------------
// Cycle counter
// (for syncing CPU with PPU/APU; incremented inside opcode handlers)
// -----------------------------------------------------------------------------
void cpu_cycles_add(int cycles); // add N CPU cycles (use inside opcode impls)
uint64_t cpu_get_cycles(void);


// -----------------------------------------------------------------------------
// Timing Hooks (PPU stepping is driven behind these)
// -----------------------------------------------------------------------------
void cpu_dma_stall(int cycles); // large one-shot stalls (e.g., $4014 OAM DMA)
int cpu_cycles_parity(void); // 0 = even cycle, 1 = odd (for 513/514 DMA)

// -----------------------------------------------------------------------------
// Register Accessors
// -----------------------------------------------------------------------------
uint16_t cpu_get_pc(void);
void cpu_set_pc(uint16_t pc);

uint8_t cpu_get_sp(void);
void cpu_set_sp(uint8_t sp);

uint8_t cpu_get_p(void);
void cpu_set_p(uint8_t p);  // implementation should ensure FLAG_U stays set

uint8_t cpu_get_a(void);
void cpu_set_a(uint8_t a);

uint8_t cpu_get_x(void);
void cpu_set_x(uint8_t x);

uint8_t cpu_get_y(void);
void cpu_set_y(uint8_t y);

// -----------------------------------------------------------------------------
// ALU Helpers
// -----------------------------------------------------------------------------
// Perform 8-bit ADC with carry into A, sets C/V/Z/N appropriately.
// Decimal mode behavior is not used on the NES (D flag ignored), but the
// helper should match 6502 binary addition semantics used by the NES CPU.
void do_adc(uint8_t m);

// -----------------------------------------------------------------------------
// Debug / tracing helpers
// -----------------------------------------------------------------------------
const char* cpu_dissasm(uint16_t pc, char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif


#endif // NES_CPU_H