// Minimal CPU bus stubs so ppu_regs.c can link in a standalone test.
#include <stdint.h>

uint8_t cpu_read(uint16_t addr) { (void)addr; return 0; }
void    cpu_write(uint16_t addr, uint8_t v) { (void)addr; (void)v; }
void    cpu_nmi(void) { /* no-op for this unit test */ }
