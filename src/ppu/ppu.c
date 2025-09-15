#include <stdint.h>

#include "ppu.h"
#include "ppu_mem.h"
#include "ppu_regs.h"

void ppu_reset(void)
{
    ppu_regs_reset();
}

uint8_t ppu_read(uint16_t cpu_addr)
{
    return ppu_regs_read(cpu_addr);
}

void ppu_write(uint16_t cpu_addr, uint8_t value)
{
    ppu_regs_write(cpu_addr, value);
}

// For early boot/testing: treat “fake vblank on/off” as set/clear VBlank.
void ppu_set_fake_vblank(int on)
{
    ppu_regs_set_vblank(on != 0);
}

// Optional timing hook (no-op for now)
void ppu_step(int cpu_cycles)
{
    (void)cpu_cycles;
}