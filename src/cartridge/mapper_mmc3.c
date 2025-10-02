// src/cartridge/mapper_mmc3.c  —  MMC3 (Mapper 4) with correct CHR mapping + level IRQ
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mapper.h"
#include "bus.h"
#include "cpu.h"
#include "ppu_mem.h"

// Forward decls in case cpu.h already has these:
void cpu_irq_assert(void);
void cpu_irq_clear(void);

// #define MMC3_TRACE 1
#if MMC3_TRACE
  #include <stdio.h>
  #define T(...) fprintf(stderr, __VA_ARGS__)
#else
  #define T(...) do{}while(0)
#endif

// ---------------------
// ROM / RAM storage
// ---------------------
static const uint8_t* g_prg = NULL; // PRG ROM (external)
static size_t g_prg_len = 0;        // multiple of 0x2000 (8KB)

static uint8_t* g_chr = NULL;       // CHR ROM/RAM (copied/allocated)
static size_t   g_chr_len = 0;      // bytes (0 => CHR-RAM 8KB)
static int      g_chr_is_ram = 0;   // 1 if CHR-RAM

static uint8_t  g_prg_ram[0x2000];  // 8KB PRG-RAM at $6000-$7FFF
static int      g_prg_ram_enable = 1; // $A001 bit7 (simplified)

// ---------------------
// Banking registers
// ---------------------
// $8000: ....CPMB (C=CHR mode, M=PRG mode, B=target)
static uint8_t bank_select = 0x00;
static uint8_t regs[8] = {0};       // $8001 bank data goes into one of these

// Effective 8KB PRG banks for CPU $8000,$A000,$C000,$E000
static int prg_bank[4] = {0,0,0,0};

// ---------------------
// IRQ (MMC3)
// ---------------------
static uint8_t irq_latch       = 0x00; // $C000
static uint8_t irq_counter     = 0x00; // internal
static uint8_t irq_enable      = 0x00; // $E001 / $E000
static uint8_t irq_reload_next = 0;    // $C001
static uint8_t irq_pending     = 0;    // latched until $E000 ack

// A12 edge detector + simple low-time filter
static uint8_t last_a12    = 0;
static uint8_t a12_low_run = 0;

// ---------------------
// Helpers
// ---------------------
static inline size_t prg_bank_count_8k(void) { return g_prg_len / 0x2000; }
static inline size_t chr_bank_count_1k(void) { return g_chr_len / 0x0400; }

static inline int clamp_prg8(int b)
{
    int max = (int)prg_bank_count_8k();
    if (max <= 0) return 0;
    if (b < 0) b = 0;
    if (b >= max) b = max - 1;
    return b;
}

static inline int mask_chr1(int b)
{
    int max = (int)chr_bank_count_1k();
    if (max == 0) return 0;
    b %= max; if (b < 0) b += max;
    return b;
}

static void update_prg_map(void)
{
    const int prg_mode = (bank_select >> 6) & 1; // M
    const int last = (int)prg_bank_count_8k() - 1;

    int r6 = regs[6] % (int)prg_bank_count_8k();
    int r7 = regs[7] % (int)prg_bank_count_8k();

    if (!prg_mode) {
        // M=0: [8000]=R6, [A000]=R7, [C000]=last-1, [E000]=last
        prg_bank[0] = clamp_prg8(r6);
        prg_bank[1] = clamp_prg8(r7);
        prg_bank[2] = clamp_prg8(last - 1);
        prg_bank[3] = clamp_prg8(last);
    } else {
        // M=1: [8000]=last-1, [A000]=R7, [C000]=R6, [E000]=last
        prg_bank[0] = clamp_prg8(last - 1);
        prg_bank[1] = clamp_prg8(r7);
        prg_bank[2] = clamp_prg8(r6);
        prg_bank[3] = clamp_prg8(last);
    }
}

// ---- FIXED CHR MAP: NO "*2" FOR 2KB WINDOWS ----
// regs[0]/regs[1] specify 1KB indices; LSB ignored (forced even).
// For 2KB regions, use (base, base+1) — not base*2.
static inline int chr_map_1k(uint16_t ppu_addr)
{
    const int chr_mode = (bank_select >> 7) & 1; // C
    const uint16_t a = ppu_addr & 0x1FFF;

    if (!chr_mode) {
        // C=0:
        //  $0000-$07FF -> 2KB regs[0] (even)
        //  $0800-$0FFF -> 2KB regs[1] (even)
        //  $1000-$13FF -> 1KB regs[2]
        //  $1400-$17FF -> 1KB regs[3]
        //  $1800-$1BFF -> 1KB regs[4]
        //  $1C00-$1FFF -> 1KB regs[5]
        if (a < 0x0800) {
            int base = (regs[0] & ~1);                     // even 1KB index
            return mask_chr1(base + ((a >> 10) & 1));      // base or base+1
        } else if (a < 0x1000) {
            int base = (regs[1] & ~1);
            return mask_chr1(base + (((a - 0x0800) >> 10) & 1));
        } else if (a < 0x1400) {
            return mask_chr1(regs[2]);
        } else if (a < 0x1800) {
            return mask_chr1(regs[3]);
        } else if (a < 0x1C00) {
            return mask_chr1(regs[4]);
        } else {
            return mask_chr1(regs[5]);
        }
    } else {
        // C=1: 1KB banks at $0000-$0FFF; the 2KB pair moves to $1000-$1FFF
        if (a < 0x0400) return mask_chr1(regs[2]);
        if (a < 0x0800) return mask_chr1(regs[3]);
        if (a < 0x0C00) return mask_chr1(regs[4]);
        if (a < 0x1000) return mask_chr1(regs[5]);
        // $1000-$17FF -> 2KB regs[0] (even), $1800-$1FFF -> regs[1] (even)
        if (a < 0x1800) {
            int base = (regs[0] & ~1);
            return mask_chr1(base + (((a - 0x1000) >> 10) & 1));
        }
        int base = (regs[1] & ~1);
        return mask_chr1(base + (((a - 0x1800) >> 10) & 1));
    }
}

// ---- IRQ: correct “reload then decrement; trigger when becomes 0” ----
static void mmc3_on_valid_a12_rise(void)
{
    // nesdev sequence on A12 rise:
    // if reload_next: counter = latch; reload_next = 0;
    // else if counter == 0: counter = latch;
    // else counter--;
    // if counter == 0 and irq_enable: request IRQ (level assert)
    if (irq_reload_next) {
        irq_counter = irq_latch;
        irq_reload_next = 0;
    } else if (irq_counter == 0) {
        irq_counter = irq_latch;
    } else {
        irq_counter--;
    }

    if (irq_counter == 0) {
        irq_pending = 1;
        if (irq_enable) {
            cpu_irq_assert();
            T("[MMC3] IRQ assert (latch=%u)\n", irq_latch);
        }
    }
}

// ---------------------
// CPU (PRG) handlers
// ---------------------
static uint8_t mmc3_cpu_read(uint16_t addr)
{
    if (addr >= 0x6000 && addr < 0x8000)
        return g_prg_ram_enable ? g_prg_ram[addr - 0x6000] : 0xFF;

    if (addr >= 0x8000) {
        int slot = (addr - 0x8000) >> 13; // 0..3 (8KB each)
        int bank = prg_bank[slot];
        size_t base = (size_t)bank * 0x2000;
        return g_prg[base + (addr & 0x1FFF)];
    }

    return cpu_read(addr);
}

static void mmc3_cpu_write(uint16_t addr, uint8_t v)
{
    if (addr >= 0x6000 && addr < 0x8000) {
        if (g_prg_ram_enable) g_prg_ram[addr - 0x6000] = v;
        return;
    }

    if (addr >= 0x8000) {
        switch (addr & 0xE001)
        {
        case 0x8000: // even: bank select
            bank_select = v;
            update_prg_map();
            return;

        case 0x8001: { // odd: bank data
            const uint8_t target = bank_select & 0x07;
            regs[target] = v;
            if (target >= 6) update_prg_map(); // PRG banks changed
            return;
        }

        case 0xA000: // even: mirroring (0=vertical, 1=horizontal)
            ppu_mem_set_mirroring((v & 1) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL);
            return;

        case 0xA001: // odd: PRG-RAM protect/enable (simplified: bit7 enables)
            g_prg_ram_enable = (v & 0x80) ? 1 : 0;
            return;

        case 0xC000: // even: IRQ latch
            irq_latch = v;
            return;

        case 0xC001: // odd: IRQ reload (on next valid A12 rise)
            irq_reload_next = 1;
            return;

        case 0xE000: // even: IRQ disable + acknowledge (clear CPU line)
            irq_enable = 0;
            irq_pending = 0;
            cpu_irq_clear();
            T("[MMC3] IRQ clear/disable\n");
            return;

        case 0xE001: // odd: IRQ enable
            irq_enable = 1;
            if (irq_pending) {
                cpu_irq_assert();  // re-assert immediately if a pending IRQ exists
                T("[MMC3] IRQ re-assert on enable\n");
            }
            return;
        }
        return;
    }

    cpu_write(addr, v);
}

// ---------------------
// PPU (CHR) handlers
// ---------------------
static uint8_t mmc3_chr_read(uint16_t addr)
{
    // A12 edge clock via CHR reads (works if your PPU fetches CHR during rendering)
    const uint8_t a12 = (addr & 0x1000) ? 1 : 0;

    // A12 low-time filter: only clock on a rising edge after A12 has been low long enough.
    if (a12) {
        if (!last_a12 && a12_low_run >= 8) {
            mmc3_on_valid_a12_rise();
        }
        last_a12 = 1;
        a12_low_run = 0;
    } else {
        last_a12 = 0;
        if (a12_low_run < 32) a12_low_run++; // saturate
    }

    int b1k = chr_map_1k(addr);
    size_t base = (size_t)b1k * 0x0400;
    return g_chr[base + (addr & 0x03FF)];
}

static void mmc3_chr_write(uint16_t addr, uint8_t v)
{
    if (g_chr_is_ram) {
        int b1k = chr_map_1k(addr);
        size_t base = (size_t)b1k * 0x0400;
        g_chr[base + (addr & 0x03FF)] = v;
    }
    (void)addr; (void)v;
}

// ---------------------
// Optional public scanline hook
// Call once per visible scanline at dot ~260 if your PPU isn't cycle-accurate.
//   if (scanline>=0 && scanline<240 && dot==260 && (ppumask&0x18)) mapper_mmc3_on_ppu_scanline_tick();
void mapper_mmc3_on_ppu_scanline_tick(void)
{
    mmc3_on_valid_a12_rise();
}

// ---------------------
// Ops table + factory
// ---------------------
static struct MapperOps mmc3_ops = {
    .cpu_read  = mmc3_cpu_read,
    .cpu_write = mmc3_cpu_write,
    .chr_read  = mmc3_chr_read,
    .chr_write = mmc3_chr_write,
};

const struct MapperOps* mapper_mmc3_init(const uint8_t* prg_data, size_t prg_len,
                                         const uint8_t* chr_data, size_t chr_len)
{
    // PRG must be multiple of 8KB, >= 32KB
    if ((prg_len % 0x2000) != 0 || prg_len < 0x8000) return NULL;

    g_prg     = prg_data;
    g_prg_len = prg_len;

    if (chr_len == 0) {
        // CHR-RAM 8KB default
        g_chr_is_ram = 1;
        g_chr_len    = 0x2000;
        g_chr        = (uint8_t*)malloc(g_chr_len);
        if (!g_chr) return NULL;
        memset(g_chr, 0, g_chr_len);
    } else {
        // CHR-ROM: copy so we can bank by 1KB index
        if ((chr_len % 0x0400) != 0) return NULL;
        g_chr_is_ram = 0;
        g_chr_len    = chr_len;
        g_chr        = (uint8_t*)malloc(g_chr_len);
        if (!g_chr) return NULL;
        memcpy(g_chr, chr_data, chr_len);
    }

    memset(regs, 0, sizeof(regs));
    bank_select = 0x00;

    irq_latch = 0;
    irq_counter = 0;
    irq_enable = 0;
    irq_reload_next = 0;
    irq_pending = 0;

    last_a12 = 0;
    a12_low_run = 0;

    update_prg_map();
    memset(g_prg_ram, 0, sizeof(g_prg_ram));
    g_prg_ram_enable = 1;

    T("[MMC3] init: PRG=%zu CHR=%zu\n", g_prg_len, g_chr_len);
    return &mmc3_ops;
}
