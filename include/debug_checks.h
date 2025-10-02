// include/debug_checks.h
#ifndef NES_DEBUG_CHECKS_H
#define NES_DEBUG_CHECKS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Your code already exposes these (based on your snippets); if names differ, adjust:
uint16_t cpu_get_pc(void);
uint8_t  cpu_read(uint16_t addr);
uint64_t cpu_get_cycles(void);   // if you don’t expose this yet, add a trivial getter from your CPU module
void     ppu_step(int cpu_cycles);
void     apu_step(int cpu_cycles);

// Call this *instead of* your normal trio (cpu_step + ppu_step + apu_step):
// You still need to call your actual cpu_step() inside the macro body; for flexibility,
// we pass it as a parameter so you can inject any call/inline you already have.
#define DBG_WRAP_STEP(CPU_STEP_CALL)                                                \
    do {                                                                            \
        uint64_t __c0 = cpu_get_cycles();                                           \
        uint16_t __pc = cpu_get_pc();                                               \
        uint8_t  __op = cpu_read(__pc); /* okay for peek */                         \
        CPU_STEP_CALL;                                                              \
        uint64_t __c1 = cpu_get_cycles();                                           \
        uint64_t __dc = (__c1 - __c0);                                              \
        if (__dc == 0) {                                                            \
            fprintf(stderr, "[BUG] cpu_step added 0 cycles at PC=%04X op=%02X\n",   \
                    (unsigned)__pc, (unsigned)__op);                                \
            abort();                                                                \
        }                                                                           \
        /* PPU/APU consume *CPU* cycles here; your ppu_step expects CPU cycles */   \
        ppu_step((int)__dc);                                                        \
        apu_step((int)__dc);                                                        \
    } while (0)

#endif // NES_DEBUG_CHECKS_H
