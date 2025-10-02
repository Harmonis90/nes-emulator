// src/ppu/ppu_regs.c
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cpu.h"
#include "bus.h"
#include "ppu_regs.h"
#include "ppu_mem.h"

// ==============================
// Logging controls
// ==============================
// 0 = silent, 1 = high-level, 2 = verbose (very spammy)
#ifndef PPU_LOG_LEVEL
#define PPU_LOG_LEVEL 1
#endif

// Cap how many log lines we emit (0 = unlimited)
#ifndef PPU_LOG_BUDGET
#define PPU_LOG_BUDGET 200
#endif

// 1 = log to ppu_trace.txt (buffered), 0 = stderr
#ifndef PPU_LOG_TO_FILE
#define PPU_LOG_TO_FILE 1
#endif

#if PPU_LOG_LEVEL > 0
static FILE* s_ppu_log = NULL;
static uint64_t s_ppu_log_budget = PPU_LOG_BUDGET;

static inline FILE* PLOG_STREAM(void) {
    if (!s_ppu_log) {
#if PPU_LOG_TO_FILE
        s_ppu_log = fopen("ppu_trace.txt", "wb");
        if (!s_ppu_log) s_ppu_log = stderr;
        else {
            // big buffer to keep it fast
            static char buf[1 << 20];
            setvbuf(s_ppu_log, buf, _IOFBF, sizeof(buf));
        }
#else
        s_ppu_log = stderr;
#endif
    }
    return s_ppu_log;
}

#define PLOG_L(level, ...) do { \
    if ((level) <= PPU_LOG_LEVEL) { \
        if (s_ppu_log_budget == 0 && PPU_LOG_BUDGET != 0) break; \
        FILE* f__ = PLOG_STREAM(); \
        if (f__) { fprintf(f__, __VA_ARGS__); } \
        if (PPU_LOG_BUDGET != 0 && s_ppu_log_budget) --s_ppu_log_budget; \
    } \
} while (0)
#else
#define PLOG_L(level, ...) do { } while (0)
#endif

#define LOG_HI(...) PLOG_L(1, __VA_ARGS__)
#define LOG_VB(...) PLOG_L(2, __VA_ARGS__)

// ==============================
// Internal state
// ==============================
typedef struct {
    uint8_t  ppuctrl;    // $2000
    uint8_t  ppumask;    // $2001
    uint8_t  ppustatus;  // $2002
    uint8_t  oamaddr;    // $2003

    uint8_t  oam[256];   // primary OAM

    uint16_t v;          // current VRAM address (15 bits)
    uint16_t t;          // temp VRAM address (15 bits)
    uint8_t  x;          // fine X (3 bits)
    uint8_t  w;          // write toggle

    uint8_t  ppudata_buffer; // buffered read for $2007
} PPURegs;

static PPURegs R;

// ==============================
// Instrumentation counters
// ==============================
static int g_dma_count = 0;            // # of $4014 DMA operations performed
static int g_oamaddr_w_count = 0;      // # of writes to $2003
static int g_oamdata_w_count = 0;      // # of writes to $2004
static int g_nmi_count = 0;            // # of NMIs actually fired
static int g_ppustatus_read_count = 0; // # of reads of $2002 (clears VBL)
static uint8_t g_last_dma_page = 0;
static uint8_t g_last_dma_oamaddr = 0;

// Track the *level* of vblank (independent of $2002 clear-on-read)
static bool g_vblank_level = false;

// Public accessors (declare in ppu.h if not already)
int     ppu_dma_count(void)           { return g_dma_count; }
int     ppu_oamaddr_write_count(void) { return g_oamaddr_w_count; }
int     ppu_oamdata_write_count(void) { return g_oamdata_w_count; }
int     ppu_nmi_count(void)           { return g_nmi_count; }
int     ppu_status_read_count(void)   { return g_ppustatus_read_count; }
uint8_t ppu_ppuctrl_get(void)         { return R.ppuctrl; }
uint8_t ppu_ppustatus_get(void)       { return R.ppustatus; }
uint8_t ppu_ppumask_get(void)         { return R.ppumask; }   // handy alias for tests/tools

// DMA info
uint8_t ppu_last_dma_page(void)    { return g_last_dma_page; }
uint8_t ppu_oamaddr_peek(void)     { return R.oamaddr; }      // reflects current OAMADDR

// ==============================
// Small getters used elsewhere
// ==============================
uint8_t ppu_ctrl_reg(void) { return R.ppuctrl; }
uint8_t ppu_mask_reg(void) { return R.ppumask; }
uint8_t const* ppu_oam_data(void) { return R.oam; }

void ppu_regs_get_scroll(uint16_t* t_out, uint8_t* fine_x_out) {
    if (t_out) *t_out = R.t;
    if (fine_x_out) *fine_x_out = R.x;
}

// Side-effect-free peek of PPUSTATUS (use this in UI/diagnostics instead of $2002)
uint8_t ppu_regs_status_peek(void) { return R.ppustatus; }

// Level accessor: “are we currently in the vblank interval?”
bool ppu_vblank_level(void) { return g_vblank_level; }

// ==============================
// Helpers
// ==============================
static inline uint16_t decode_reg(uint16_t cpu_addr) {
    // accept lo3 (0..7) or full $2000-$3FFF addr
    if (cpu_addr <= 7u) return (uint16_t)(0x2000u + cpu_addr);
    return (uint16_t)(0x2000u + ((cpu_addr - 0x2000u) & 7u));
}

static inline uint16_t inc_amount(void) {
    return (R.ppuctrl & 0x04) ? 32u : 1u; // PPUCTRL bit2
}

// Test/helpers with range checking
uint8_t ppu_regs_oam_peek(int index)
{
    assert(index >= 0 && index < 256);
    return R.oam[(size_t)index];
}

void ppu_regs_oam_poke(int index, uint8_t value)
{
    assert(index >= 0 && index < 256);
    R.oam[(size_t)index] = value;
}

// ==============================
// VBlank + NMI
// ==============================
void ppu_regs_set_vblank(bool on) {
    uint8_t before = R.ppustatus;
    g_vblank_level = on; // keep a level signal for UI/timing

    if (on) {
        if (!(R.ppustatus & 0x80)) {
            R.ppustatus |= 0x80;
            if (R.ppuctrl & 0x80) {
                LOG_HI("NMI (VBL rising and NMI enabled)\n");
                cpu_nmi();
                g_nmi_count++;
            }
        }
    } else {
        R.ppustatus &= (uint8_t)~0x80;
    }

    if (((before ^ R.ppustatus) & 0x80)) {
        LOG_VB("PPUSTATUS VBL %s\n", (R.ppustatus & 0x80) ? "SET" : "CLR");
    }
}

// VBlank flag accessor for tests/tools (note: this reflects $2002 bit7 and is clear-on-read)
bool ppu_in_vblank(void) { return (R.ppustatus & 0x80u) != 0; }

// ==============================
// Reset
// ==============================
void ppu_regs_reset(void) {
    memset(&R, 0, sizeof R);
    // Power-up-ish: many emus set bit4 from "open bus" power-on pattern
    R.ppustatus = 0x10;

    // reset counters too
    g_dma_count = 0;
    g_oamaddr_w_count = 0;
    g_oamdata_w_count = 0;
    g_nmi_count = 0;
    g_ppustatus_read_count = 0;
    g_last_dma_page = 0;
    g_last_dma_oamaddr = 0;
    g_vblank_level = false;

    LOG_HI("PPU regs reset\n");
}

// ==============================
// Read handlers
// ==============================
static uint8_t read_2002(void) {
    g_ppustatus_read_count++;       // track clear-on-read usage
    uint8_t val = R.ppustatus;
    LOG_HI("PPUSTATUS read => %02X (VBL=%d)\n", val, (val & 0x80) ? 1 : 0);
    R.ppustatus &= (uint8_t)~0x80;  // clear VBL
    R.w = 0;                        // reset write toggle
    return val;
}

static uint8_t read_2004(void) {
    return R.oam[R.oamaddr];
}

static uint8_t read_2007(void) {
    uint16_t addr = (uint16_t)(R.v & 0x3FFF);
    uint8_t data = ppu_mem_read(addr);
    uint8_t ret = (addr < 0x3F00) ? R.ppudata_buffer : data;
    if (addr < 0x3F00) R.ppudata_buffer = data;

    R.v = (uint16_t)((R.v + inc_amount()) & 0x7FFF);
    return ret;
}

// ==============================
// Write handlers
// ==============================
static void write_2000(uint8_t v) {
    uint8_t prev = R.ppuctrl;
    R.ppuctrl = v;

    // Update t nametable bits (10,11)
    R.t = (uint16_t)((R.t & ~0x0C00u) | (((uint16_t)(v & 0x03)) << 10));

    LOG_HI("PPUCTRL <= %02X (NMI=%d, inc=%d, sprTbl=%d, bgTbl=%d, sprSz=%d, nt=%d)\n",
           v, (v >> 7) & 1, (v >> 2) & 1, (v >> 3) & 1, (v >> 4) & 1, (v >> 5) & 1, v & 3);

    // If NMI became enabled while VBL is already set, fire NMI now.
    if ((~prev & v) & 0x80) {
        if (R.ppustatus & 0x80) {
            LOG_HI("NMI (armed earlier while VBL=0)\n");
            cpu_nmi();
            g_nmi_count++;
        }
    }
}

static void write_2001(uint8_t v) {
    R.ppumask = v;
    LOG_HI("PPUMASK <= %02X (grayscale=%d, showBG=%d, showSPR=%d, emphRGB=%d%d%d)\n",
           v,
           (v & 0x01) != 0,
           (v & 0x08) != 0,
           (v & 0x10) != 0,
           (v & 0x20) != 0, (v & 0x40) != 0, (v & 0x80) != 0);
}

static void write_2003(uint8_t v) {
    R.oamaddr = v;
    g_oamaddr_w_count++;
}

static void write_2004(uint8_t v) {
    R.oam[R.oamaddr++] = v; // OAMADDR auto-increments, wraps naturally
    g_oamdata_w_count++;
}

static void write_2005(uint8_t v) {
    if (R.w == 0) {
        R.x = (uint8_t)(v & 7);
        R.t = (uint16_t)((R.t & ~0x001Fu) | ((uint16_t)(v >> 3) & 0x1F));
        R.w = 1;
        LOG_VB("PPUSCROLL <= %02X (w=1, t=$%04X, x=%d)\n", v, R.t, R.x);
    } else {
        R.t = (uint16_t)((R.t & ~0x7000u) | (((uint16_t)(v & 0x07)) << 12)); // fine Y
        R.t = (uint16_t)((R.t & ~0x03E0u) | (((uint16_t)(v & 0xF8)) << 2));  // coarse Y
        R.w = 0;
        LOG_VB("PPUSCROLL <= %02X (w=0, t=$%04X, x=%d)\n", v, R.t, R.x);
    }
}

static void write_2006(uint8_t v) {
    if (R.w == 0) {
        R.t = (uint16_t)((R.t & 0x00FFu) | (((uint16_t)(v & 0x3F)) << 8));
        R.w = 1;
        LOG_VB("PPUADDR <= %02X (v=$%04X, w=1)\n", v, R.v);
    } else {
        R.t = (uint16_t)((R.t & 0x7F00u) | (uint16_t)v);
        R.v = R.t;
        R.w = 0;
        LOG_VB("PPUADDR <= %02X (v=$%04X, w=0)\n", v, R.v);
    }
}

static void write_2007(uint8_t value) {
    uint16_t addr = (uint16_t)(R.v & 0x3FFF);
    ppu_mem_write(addr, value);
    R.v = (uint16_t)((R.v + inc_amount()) & 0x7FFF);
}

// ==============================
// Public bus entry points
// ==============================
uint8_t ppu_regs_read(uint16_t cpu_addr) {
    uint16_t reg = decode_reg(cpu_addr);
    switch (reg) {
        case 0x2002: return read_2002(); // PPUSTATUS
        case 0x2004: return read_2004(); // OAMDATA
        case 0x2007: return read_2007(); // PPUDATA
        default:
            LOG_VB("PPU read from write-only reg $%04X (return 0)\n", reg);
            return 0x00;
    }
}

void ppu_regs_write(uint16_t cpu_addr, uint8_t value) {
    uint16_t reg = decode_reg(cpu_addr);
    switch (reg) {
        case 0x2000: write_2000(value); break; // PPUCTRL
        case 0x2001: write_2001(value); break; // PPUMASK
        case 0x2003: write_2003(value); break; // OAMADDR
        case 0x2004: write_2004(value); break; // OAMDATA
        case 0x2005: write_2005(value); break; // PPUSCROLL
        case 0x2006: write_2006(value); break; // PPUADDR
        case 0x2007: write_2007(value); break; // PPUDATA
        default:
            LOG_VB("PPU write to read-only/unused reg $%04X <= $%02X\n", reg, value);
            break;
    }
}

// ==============================
// OAM DMA ($4014)
// ==============================
void ppu_oam_dma(uint8_t page) {
    g_dma_count++;  // count exactly once per DMA
    g_last_dma_page    = page;
    g_last_dma_oamaddr = R.oamaddr;

    uint16_t base = ((uint16_t)page) << 8;

#if PPU_LOG_LEVEL >= 1
    // Dump just the first few entries so it’s cheap
    uint8_t s0 = cpu_read((uint16_t)(base + 0));
    uint8_t s1 = cpu_read((uint16_t)(base + 1));
    uint8_t s2 = cpu_read((uint16_t)(base + 2));
    uint8_t s3 = cpu_read((uint16_t)(base + 3));
    uint8_t s4 = cpu_read((uint16_t)(base + 4));
    uint8_t s5 = cpu_read((uint16_t)(base + 5));
    uint8_t s6 = cpu_read((uint16_t)(base + 6));
    uint8_t s7 = cpu_read((uint16_t)(base + 7));
    LOG_HI("OAM DMA from $%04X  src[0..7]=%02X%02X%02X%02X%02X%02X%02X%02X  OAMADDR=%02X\n",
           base, s0,s1,s2,s3,s4,s5,s6,s7, R.oamaddr);
#endif

    // DMA behaves like 256 writes to $2004, starting at OAMADDR and wrapping
    uint8_t addr = R.oamaddr;
    for (int i = 0; i < 256; ++i) {
        R.oam[addr] = cpu_read((uint16_t)(base + i));
        addr = (uint8_t)(addr + 1);
    }
    R.oamaddr = addr;

#if PPU_LOG_LEVEL >= 1
    int visible = 0;
    for (int i = 0; i < 64; ++i) {
        uint8_t y = R.oam[i*4 + 0];
        if (y < 0xEF) visible++;
    }
    LOG_HI("DMA OAM: visible=%d (end OAMADDR=%02X)\n", visible, R.oamaddr);
#endif
}
