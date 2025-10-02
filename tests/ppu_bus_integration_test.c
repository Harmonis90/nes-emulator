// tests/ppu_bus_integration_test.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "ppu_regs.h"   // ppu_regs_write/read, ppu_ctrl_reg, ppu_regs_oam_poke/peek
#include "ppu_mem.h"    // ppu_mem_read/write, mirroring_t

/* ============================================================
   Minimal stubs so we can link PPU modules standalone
   ============================================================ */

/* --- Fake CPU bus (only RAM + PPU regs + $4014 DMA) --- */
static uint8_t s_ram[0x800];  // 2KB CPU RAM (mirrored)
uint8_t cpu_read(uint16_t addr) {
    // For this test we only need RAM reads for DMA.
    if (addr < 0x2000) return s_ram[addr & 0x07FF];
    return 0;
}
void    cpu_write(uint16_t addr, uint8_t v) {
    if (addr < 0x2000) { s_ram[addr & 0x07FF] = v; return; }
    // Map CPU writes to PPU regs mirrors ($2000-$3FFF):
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        uint16_t lo3 = (uint16_t)((addr - 0x2000u) & 7u);
        ppu_regs_write(lo3, v);
        return;
    }
    // Handle $4014 OAM DMA: copy 256 bytes from page -> OAM (start at current OAMADDR)
    if (addr == 0x4014) {
        extern void ppu_regs_oam_poke(uint8_t index, uint8_t v);
        extern uint8_t ppu_regs_oam_peek(uint8_t index);
        // We'll start at current OAMADDR if your regs track it; if not, start at 0.
        // To keep this test independent, just start at 0 (SMB sets OAMADDR=0 anyway).
        uint16_t base = ((uint16_t)v) << 8;
        uint8_t idx = 0;
        for (int i = 0; i < 256; ++i) {
            ppu_regs_oam_poke(idx++, cpu_read((uint16_t)(base + i)));
        }
        (void)ppu_regs_oam_peek(0); // touch symbol to ensure it links
        return;
    }
}
void cpu_nmi(void) { /* not used in this test */ }

/* --- Mapper CHR hooks used by ppu_mem.c --- */
static uint8_t g_chr[0x2000];  // 8KB CHR-RAM for test
uint8_t mapper_chr_read(uint16_t addr) { return g_chr[addr & 0x1FFF]; }
void    mapper_chr_write(uint16_t addr, uint8_t v) { g_chr[addr & 0x1FFF] = v; }

/* --- Nametable mirroring query used by ppu_mem.c --- */
//mirroring_t ppu_mem_get_mirroring(void) { return MIRROR_HORIZONTAL; }

/* ============================================================
   Tests
   ============================================================ */

static int test_ppuctrl_mirror_write(void) {
    // Write PPUCTRL through a MIRROR ($2008) and verify latch sees it.
    uint8_t v = 0x28; // enable 8x16 (bit 5) and select sprite table 0x0000 (bit 3=0)
    cpu_write(0x2008, v); // mirror of $2000
    uint8_t got = ppu_ctrl_reg();
    if (got != v) {
        fprintf(stderr, "PPUCTRL mirror write FAILED: wrote $%02X via $2008, got $%02X\n", v, got);
        return 1;
    }
    fprintf(stderr, "PPUCTRL mirror write OK: $%02X\n", got);
    return 0;
}

static int test_oam_dma(void) {
    // Fill CPU RAM page $0200..$02FF with a pattern, trigger $4014=0x02, and verify OAM.
    for (int i = 0; i < 256; ++i) s_ram[0x200 + i] = (uint8_t)i;
    cpu_write(0x4014, 0x02);  // DMA from $0200
    // Spot-check a few bytes in OAM:
    extern uint8_t ppu_regs_oam_peek(uint8_t index);
    int fail = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t o = ppu_regs_oam_peek((uint8_t)i);
        if (o != (uint8_t)i) { fprintf(stderr, "OAM DMA FAIL at %d: got %02X expect %02X\n", i, o, (uint8_t)i); fail = 1; }
    }
    if (!fail) fprintf(stderr, "OAM DMA OK (first 8 bytes match)\n");
    return fail;
}

static int test_chr_through_mapper(void) {
    // Writes < $2000 must land in CHR-RAM via mapper_chr_write, and read back via mapper_chr_read.
    memset(g_chr, 0, sizeof(g_chr));
    ppu_mem_write(0x0000, 0xAA);
    ppu_mem_write(0x1FFF, 0x55);
    uint8_t a = ppu_mem_read(0x0000);
    uint8_t b = ppu_mem_read(0x1FFF);
    int fail = 0;
    if (a != 0xAA) { fprintf(stderr, "CHR path FAIL at $0000: got %02X expect AA\n", a); fail = 1; }
    if (b != 0x55) { fprintf(stderr, "CHR path FAIL at $1FFF: got %02X expect 55\n", b); fail = 1; }
    if (!fail) fprintf(stderr, "CHR path OK ($0000=$AA, $1FFF=$55)\n");
    return fail;
}

int main(void) {
    int rc = 0;
    rc |= test_ppuctrl_mirror_write();
    rc |= test_oam_dma();
    rc |= test_chr_through_mapper();
    fprintf(stderr, "PPU bus integration: %s\n", rc ? "FAIL" : "PASS");
    return rc;
}
