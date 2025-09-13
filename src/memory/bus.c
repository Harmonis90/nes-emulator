#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bus.h"
#include "controller.h"
#include "mapper.h"
#include "ppu.h"


// -------------------------
// Memory backing
// -------------------------
static uint8_t cpu_ram[0x0800];  // 2KB internal ram
static uint8_t prg_ram[0x2000];  // 8KB PRG-RAM ($6000-$7FFF), optional
static uint8_t prg_rom[0x8000];  // up to 32KB PRG mapped at $8000-$FFFF

// Size of valid PRG data in prg_rom[]: 0x4000 (16KB) or 0x8000 (32KB).
// For our test harness we default to 32KB so writes to $8000 work without loader.
static size_t prg_size_bytes = 0x8000;

// Allow writes to $8000-$FFFF for the test harness (acts like RAM).
// Set this to 0 if you later load a real ROM and want true read-only behavior.
static int debug_allow_write_rom = 1;

// One-time warnings to keep logs clean.
static int warned_ppu = 0, warned_apu = 0, warned_test = 0;

// -------------------------
// Helpers
// -------------------------
static inline uint8_t prg_read(uint16_t addr)
{
    // addr in $8000-$FFFF
    if (prg_size_bytes == 0x4000)
    {
        size_t idx = (addr - 0x8000) & 0x3FFF;
        return prg_rom[idx];
    }
    else
    {
        size_t idx = (addr - 0x8000) & 0x7FFF;
        return prg_rom[idx];
    }
}

static inline void prg_write(uint16_t addr, uint8_t val)
{
    if (!debug_allow_write_rom) return;
    if (prg_size_bytes == 0x4000)
    {
        size_t idx = (addr - 0x8000) & 0x3FFF;
        prg_rom[idx] = val;
    }
    else
    {
        size_t idx = (addr - 0x8000) & 0x7FFF;
        prg_rom[idx] = val;
    }
}

// -------------------------
// Public API
// -------------------------
void bus_reset(void)
{
    memset(cpu_ram, 0, sizeof(cpu_ram));
    memset(prg_ram, 0, sizeof(prg_ram));
    memset(prg_rom, 0, sizeof(prg_rom));

    prg_size_bytes = 0x8000;
    debug_allow_write_rom = 1;
    warned_ppu = warned_apu = warned_test = 0;
}

// Optional helpers if you later add a loader:
void bus_load_prg(const uint8_t *data, size_t len)
{
    if (!data || (len != 0x4000 && len != 0x8000))
    {
        if (!warned_test)
        {
            fprintf(stderr, "bus: invalid PRG size %zu (expected 16KB/32KB). Using debug buffer.\n", len);
            warned_test = 1;
        }
        return;
    }
    memcpy(prg_rom, data, len);
    if (len == 0x4000)
    {
        memcpy(prg_rom + 0x4000, prg_rom, 0x4000);
    }
    prg_size_bytes = len;
    debug_allow_write_rom = 0;
}

void bus_set_prg_size(size_t sz_bytes)
{
    // For tests: force mirroring behavior without a ROM file.
    if (sz_bytes == 0x4000 || sz_bytes == 0x8000)
    {
        prg_size_bytes = sz_bytes;
    }
}

void bus_set_rom_write_enabled(int on)
{
    debug_allow_write_rom = on ? 1 : 0;
}

uint8_t cpu_read(uint16_t addr)
{
    // $0000-$1FFF: 2KB RAM, mirrored every $0800
    if (addr <= 0x1FFF)
    {
        return cpu_ram[addr & 0x07FF];
    }
    // $2000-$3FFF: PPU registers (mirrored every 8) — stubbed
    if (addr >= 0x2000 && addr <= 0x3FFF)
    {
       return ppu_read(addr);
    }
    // $4000-$4017: APU/IO — stubbed
    if (addr >= 0x4000 && addr <= 0x4017)
    {
        if (addr == 0x4016 || addr == 0x4017)
        {
            return controller_read(addr);
        }

        if (!warned_apu)
        {
            fprintf(stderr, "APU/IO reads return 0 (first hit at %04X)\n", addr);
            warned_apu = 1;
        }
        return 0x00;
    }

    // $4018-$401F: disabled/test — return 0
    if (addr >= 0x4018 && addr <= 0x401F)
    {
        return 0x00;
    }

    // $6000-$7FFF: PRG-RAM (if present)
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        return prg_ram[addr - 0x6000];
    }

    // $8000-$FFFF: PRG (ROM in release; writable in debug mode)
    if (addr >= 0x8000)
    {
        return mapper_cpu_read(addr);
    }

    // Should never happen
    return 0x00;
}

void cpu_write(uint16_t addr, uint8_t val)
{
    // $0000-$1FFF: 2KB RAM, mirrored
    if (addr <= 0x1FFF)
    {
        cpu_ram[addr & 0x07FF] = val;
        return;
    }

    // $2000-$3FFF: PPU regs (ignored for now)
    if (addr >= 0x2000 && addr <= 0x3FFF)
    {
        ppu_write(addr, val);
        return;
    }

    // $4000-$4017: APU/IO (ignored for now)
    if (addr >= 0x4000 && addr <= 0x4017)
    {
        if (addr == 0x4016)
        {
            controller_write(addr, val);
            return;
        }
        if (!warned_apu)
        {
            fprintf(stderr, "Unhandled APU/IO write at %04X = %02X\n", addr, val);
            warned_apu = 1;
        }
        return;
    }

    // $4018-$401F: disabled/test
    if (addr >= 0x4018 && addr <= 0x401F)
    {
        return;
    }

    // $6000-$7FFF: PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        prg_ram[addr - 0x6000] = val;
        return;
    }

    // $8000-$FFFF: PRG window
    if (addr >= 0x8000)
    {
        mapper_cpu_write(addr, val);
        return;
    }

}