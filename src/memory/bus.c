// src/bus.c
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "mapper.h"
#include "controller.h"
#include "ppu_regs.h"
// #include "apu.h"  // uncomment once you add real APU read/write

// -------------------------
// Internal memory
// -------------------------
#define CPU_RAM_SIZE 0x0800  // 2KB internal RAM
static uint8_t s_cpu_ram[CPU_RAM_SIZE];

#define PRG_RAM_SIZE 0x2000  // 8KB PRG-RAM at $6000-$7FFF (optional on real carts)
static uint8_t s_prg_ram[PRG_RAM_SIZE];

// -------------------------
// Bus init/reset
// -------------------------
void bus_reset(void) {
    memset(s_cpu_ram, 0, sizeof s_cpu_ram);
    memset(s_prg_ram, 0, sizeof s_prg_ram);

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
        uint16_t reg = (uint16_t)(0x2000u + (addr & 0x0007u));
        return ppu_read(reg);
    }

    // $4000-$4017: APU + I/O
    if (addr >= APU_IO_START && addr <= APU_IO_END) {

        if (addr == 0x4016 || addr == 0x4017) {
            return controller_read(addr);
        }
        // reads of other APU regs / $4014 typically return open bus; 0x00 is fine for now
        return 0x00;
    }

    // $4018-$401F: disabled/test regs
    if (addr >= 0x4018 && addr <= 0x401F) {
        return 0x00;
    }

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
        return;
    }

    // $2000-$3FFF: PPU registers, mirrored every 8 bytes
    if (addr >= PPU_REG_START && addr <= PPU_REG_END) {
        uint16_t reg = (uint16_t)(0x2000u + (addr & 0x0007u));
        ppu_write(reg, data);
        return;
    }

    // $4000-$4017: APU + I/O
    if (addr >= APU_IO_START && addr <= APU_IO_END) {
        if (addr == 0x4014) {            // OAM DMA page
            ppu_oam_dma(data);
            // DMA stalls the CPU for 513 or 514 cycles (depends on current CPU cycle parity)
            int add = 513 + (cpu_cycles_parity() & 1);
            cpu_dma_stall(add);
            return;
        }
        if (addr == 0x4016 || addr == 0x4017) {
            controller_write(addr, data);
            return;
        }
        // TODO: apu_write(addr, data) once implemented
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
