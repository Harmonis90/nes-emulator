#include <stdint.h>
#include <string.h>
#include "mapper.h"
#include "bus.h"

// --- PRG (CPU space $8000-$FFFF) ---
static uint8_t prg[0x8000];   // up to 32KB
static size_t  prg_size = 0;  // 0x4000 or 0x8000

// --- CHR (PPU space $0000-$1FFF) ---
static uint8_t chr[0x2000];   // 8KB
static size_t  chr_size = 0;  // 0 or 0x2000
static int     chr_is_ram = 0;

// ---- CPU handlers (PRG) ----
static uint8_t nrom_cpu_read(uint16_t addr)
{
    if (addr >= 0x8000) {
        size_t idx = (prg_size == 0x4000)
                   ? ((addr - 0x8000) & 0x3FFF)   // mirror 16KB across 32KB window
                   :  (addr - 0x8000);
        return prg[idx];
    }
    // delegate everything below $8000 to the system bus (RAM, I/O, etc.)
    return cpu_read(addr);
}

static void nrom_cpu_write(uint16_t addr, uint8_t v)
{
    if (addr >= 0x8000) {
        // PRG is ROM on NROM; ignore writes
        return;
    }
    cpu_write(addr, v);
}

// ---- PPU handlers (CHR) ----
static uint8_t nrom_chr_read(uint16_t addr)
{
    return chr[addr & 0x1FFF];
}

static void nrom_chr_write(uint16_t addr, uint8_t v)
{
    if (chr_is_ram) {
        chr[addr & 0x1FFF] = v;
    }
    (void)v; // ignored when CHR is ROM
}

// ---- ops table ----
static struct MapperOps nrom_ops = {
    .cpu_read  = nrom_cpu_read,
    .cpu_write = nrom_cpu_write,
    .chr_read  = nrom_chr_read,
    .chr_write = nrom_chr_write,
};

// ---- factory ----
const struct MapperOps* mapper_nrom_init(const uint8_t* prg_data, size_t prg_len,
                                         const uint8_t* chr_data, size_t chr_len)
{
    // PRG must be 16KB or 32KB
    if (prg_len != 0x4000 && prg_len != 0x8000) return NULL;

    // Load PRG; mirror if 16KB
    memcpy(prg, prg_data, prg_len);
    if (prg_len == 0x4000) {
        memcpy(prg + 0x4000, prg, 0x4000);
    }
    prg_size = prg_len;

    // CHR: 0 => CHR-RAM 8KB, 0x2000 => CHR-ROM 8KB
    if (chr_len == 0) {
        memset(chr, 0, sizeof chr);
        chr_size  = sizeof chr;
        chr_is_ram = 1;
    } else if (chr_len == 0x2000) {
        memcpy(chr, chr_data, 0x2000);
        chr_size  = 0x2000;
        chr_is_ram = 0;
    } else {
        return NULL; // unsupported CHR size
    }

    return &nrom_ops;
}
