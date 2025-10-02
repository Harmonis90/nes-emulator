// src/bus.c
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "mapper.h"
#include "controller.h"
#include "ppu_regs.h"
#include "apu.h"

// -------------------------
// Internal memory
// -------------------------
#define CPU_RAM_SIZE 0x0800  // 2KB internal RAM
static uint8_t s_cpu_ram[CPU_RAM_SIZE];

#define PRG_RAM_SIZE 0x2000  // 8KB PRG-RAM at $6000-$7FFF (optional on real carts)
static uint8_t s_prg_ram[PRG_RAM_SIZE];

// -------------------------
// Instrumentation
// -------------------------
static int g_io_4014_w_count = 0;        // # of writes to $4014
static int g_wram_0200_02FF_w_count = 0; // # of writes to sprite buffer $0200-$02FF

int bus_io_4014_write_count(void)        { return g_io_4014_w_count; }
int bus_wram_spritebuf_write_count(void) { return g_wram_0200_02FF_w_count; }

// -------------------------
// Bus init/reset
// -------------------------
void bus_reset(void) {
    memset(s_cpu_ram, 0, sizeof s_cpu_ram);
    memset(s_prg_ram, 0, sizeof s_prg_ram);
    g_io_4014_w_count = 0;
    g_wram_0200_02FF_w_count = 0;
}

// Optional API (kept to satisfy header; not required for mapper 0)
void bus_set_prg_size(size_t sz_bytes) {
    (void)sz_bytes;
}

// -------------------------
// CPU reads
// -------------------------
uint8_t cpu_read(uint16_t addr) {
    // $0000-$1FFF: 2KB RAM, mirrored every $0800
    if (addr <= CPU_RAM_END) {
        return s_cpu_ram[addr & (CPU_RAM_SIZE - 1)];
    }

    // $2000-$3FFF: PPU registers, mirrored every 8 bytes
    if (addr >= PPU_REG_START && addr <= PPU_REG_END) {
        uint16_t lo3 = (uint16_t)((addr - 0x2000u) & 7u);
        return ppu_regs_read(lo3);  // call regs directly
    }

    // $4000-$4017: APU + I/O
    if (addr >= APU_IO_START && addr <= APU_IO_END) {
        if (addr == 0x4015) { return apu_read(addr); }
        if (addr == 0x4016 || addr == 0x4017) {
            return controller_read(addr);
        }
        // Other APU reads: open bus-ish for now
        return 0x00;
    }

    // $4018-$401F: disabled/test regs
    if (addr >= 0x4018 && addr <= 0x401F) {
        return 0x00;
    }
    if (addr <= 0x5FFF)
        return mapper_cpu_read(addr);
    // $6000-$7FFF: PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        return s_prg_ram[addr - 0x6000];
    }

    // $8000-$FFFF: cartridge space via active mapper
    return mapper_cpu_read(addr);
}

// -------------------------
// CPU writes
// -------------------------
void cpu_write(uint16_t addr, uint8_t data) {
    // $0000-$1FFF: 2KB RAM, mirrored
    if (addr <= CPU_RAM_END) {
        s_cpu_ram[addr & (CPU_RAM_SIZE - 1)] = data;

        // Instrument sprite buffer writes ($0200-$02FF)
        if (addr >= 0x0200 && addr <= 0x02FF) {
            g_wram_0200_02FF_w_count++;
        }
        return;
    }

    // $2000-$3FFF: PPU registers, mirrored every 8 bytes
    if (addr >= PPU_REG_START && addr <= PPU_REG_END) {
        uint16_t lo3 = (uint16_t)((addr - 0x2000u) & 7u);
        ppu_regs_write(lo3, data);  // call regs directly
        return;
    }

    // $4000-$4017: APU + I/O
    if (addr >= APU_IO_START && addr <= APU_IO_END) {
        if (addr == 0x4014) {                 // OAM DMA
            g_io_4014_w_count++;
            ppu_oam_dma(data);
            // Stall the CPU ~513/514 cycles. Your current implementation
            // only adds CPU cycles (doesn't tick PPU/APU), which is fine short-term.
            // We can improve this later if needed.
            int add = 513 + (cpu_cycles_parity() & 1);
            cpu_dma_stall(add);               // currently just cpu_cycles_add(...)
            return;
        }
        if (addr == 0x4016 || addr == 0x4017) {
            controller_write(addr, data);
            return;
        }
        if (addr >= 0x4000 && addr <= 0x4017) {
            apu_write(addr, data);
            return;
        }
        return;
    }

    // $4018-$401F: disabled/test regs
    if (addr >= 0x4018 && addr <= 0x401F) {
        return;
    }

    // $6000-$7FFF: PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        s_prg_ram[addr - 0x6000] = data;
        return;
    }

    // $8000-$FFFF: cartridge space via active mapper
    mapper_cpu_write(addr, data);
}
