#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "mapper.h"

// NROM state
static uint8_t* s_prg = NULL; // PRG ROM base
static size_t s_prg_size = 0; // 16KB (NROM-128) or 32KB (NROM-256)

static uint8_t* s_chr = NULL; // CHR base (ROM or RAM)
static size_t s_chr_size = 0; // 0 => allocate 8KB RAM; else expect 8KB
static int s_chr_is_ram = 0;

static uint8_t* s_chr_ram = NULL; // allocated when chr_size==0 or chr_is_ram

static uint8_t s_prg_ram[0x2000]; // $6000 - $7FFF

// --- NROM helpers -------------------------------------------------------------
static inline uint8_t prg_read(uint16_t addr)
{
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        return s_prg_ram[addr - 0x6000];
    }
    if (addr >= 0x8000)
    {
        uint32_t off;
        if (s_prg_size == 0x4000)
        {
            // 16KB mirrored twice over $8000-$FFFF
            off = (addr - 0x8000) & 0x3FFF;
        }
        else
        {
            // 32KB flat
            off = (addr - 0x8000) & 0x7FFF;
        }
        return s_prg[off];
    }
    return 0xFF;
}

static inline void prg_write(uint16_t addr, uint8_t data)
{
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        s_prg_ram[addr - 0x6000] = data;
        return;
    }
    // $8000+ is PRG ROM on NROM
    (void)addr; (void)data;
}

// CHR helpers ($0000-$1FFF)
static inline uint8_t chr_read(uint16_t addr)
{
    addr &= 0x1FFF;
    if (s_chr_is_ram || s_chr_ram)
    {
        return (s_chr_ram ? s_chr_ram : s_chr)[addr & 0x1FFF];
    }
    else
    {
        // CHR ROM
        return s_chr[addr & 0x1FFF];
    }
}

static inline void chr_write(uint16_t addr, uint8_t data)
{
    addr &= 0x1FFF;
    if (s_chr_is_ram || s_chr_ram)
    {
        (s_chr_ram ? s_chr_ram : s_chr)[addr & 0x1FFF] = data;
    }
    else
    {
        // ignore writes to CHR ROM
        (void)data;
    }
}

// --- Ops table ----------------------------------------------------------------
static void nrom_reset(void)
{
    memset(s_prg_ram, 0, sizeof(s_prg_ram));
}

static uint8_t nrom_cpu_read(uint16_t addr) { return prg_read(addr); }
static void nrom_cpu_write(uint16_t addr, uint8_t data) { prg_write(addr, data); }
static uint8_t nrom_chr_read(uint16_t addr) { return chr_read(addr); }
static void nrom_chr_write(uint16_t addr, uint8_t data) { chr_write(addr, data); }

static const struct
{
    uint8_t (*cpu_read)(uint16_t);
    void (*cpu_write)(uint16_t, uint8_t);
    uint8_t (*chr_read)(uint16_t);
    void (*chr_write)(uint16_t, uint8_t);
    void (*reset)(void);
} NROM_OPS = {
    nrom_cpu_read,
    nrom_cpu_write,
    nrom_chr_read,
    nrom_chr_write,
    nrom_reset,
};

// Expose to mapper.c
const void* mapper_nrom_get_ops(void)
{
    return &NROM_OPS;
}

void nrom_set_blobs(uint8_t* prg, size_t prg_size,
                    uint8_t* chr, size_t chr_size,
                    int chr_is_ram)
{
    // Free any prior CHR-RAM
    if (s_chr_ram) { free(s_chr_ram); s_chr_ram = NULL; }

    s_prg = prg;
    s_prg_size = prg_size;
    s_chr = chr;
    s_chr_size = chr_size;
    s_chr_is_ram = chr_is_ram;

    // If no CHR blob provided (or explicitly RAM), allocate 8KB RAM:
    if (s_chr_size == 0 || s_chr_is_ram)
    {
        s_chr_ram = (uint8_t*)malloc(0x2000);
        if (s_chr_ram) memset(s_chr_ram, 0, 0x2000);
        // Point CHR to RAM if we want writes
        if (s_chr_is_ram || !s_chr) s_chr = s_chr_ram;
        s_chr_size = 0x2000;
    }
}