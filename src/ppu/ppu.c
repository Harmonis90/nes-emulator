// src/ppu/ppu.c
#include <stdint.h>
#include <stdbool.h>

#include "ppu.h"
#include "ppu_mem.h"
#include "ppu_regs.h"

void ppu_reset(void)
{
    ppu_regs_reset();
    ppu_timing_reset();
}

uint8_t ppu_read(uint16_t cpu_addr)
{
    // Accept any address; mirror $2000–$3FFF down to 8 regs and defer to regs
    if (cpu_addr >= 0x2000 && cpu_addr <= 0x3FFF) {
        uint16_t lo3 = (uint16_t)((cpu_addr - 0x2000u) & 7u);
        return ppu_regs_read(lo3);
    }
    return 0;
}

void ppu_write(uint16_t cpu_addr, uint8_t value)
{
    // Accept any address; mirror $2000–$3FFF down to 8 regs and defer to regs
    if (cpu_addr >= 0x2000 && cpu_addr <= 0x3FFF) {
        uint16_t lo3 = (uint16_t)((cpu_addr - 0x2000u) & 7u);
        ppu_regs_write(lo3, value);
        return;
    }
}

// For early boot/testing: treat “fake vblank on/off” as set/clear VBlank.
void ppu_set_fake_vblank(int on)
{
    ppu_regs_set_vblank(on != 0);
}

// Optional timing hook (no-op for now)
// void ppu_step(int cpu_cycles)
// {
//     (void)cpu_cycles;
// }
