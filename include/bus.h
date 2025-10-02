//
// Created by Seth on 8/30/2025.
//

#ifndef NES_BUS_H
#define NES_BUS_H

#include <stdint.h>

// ---- CPU-visible bus API ----
uint8_t cpu_read(uint16_t addr);
void cpu_write(uint16_t addr, uint8_t data);

void bus_reset(void);
void bus_set_prg_size(size_t sz_bytes);

// Useful constants for the CPU memory map
enum
{
    CPU_RAM_START = 0x0000,
    CPU_RAM_END = 0x1FFF,

    PPU_REG_START = 0x2000,
    PPU_REG_END = 0x3FFF,

    APU_IO_START = 0x4000,
    APU_IO_END = 0x4017,

    CART_START = 0x4020,
    CART_END = 0xFFFF,

    VEC_NMI = 0xFFFA,
    VEC_RESET = 0xFFFC,
    VEC_IRQ_BRK = 0xFFFE
};
// Debug counters for tests
int bus_io_4014_write_count(void);
int bus_wram_spritebuf_write_count(void);

#endif //NES_BUS_H
