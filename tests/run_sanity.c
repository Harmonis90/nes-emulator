// tests/run_sanity.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ines.h"
#include "bus.h"
#include "ppu.h"
#include "apu.h"
#include "cpu.h"
#include "nes.h"

// If your core exposes this; otherwise remove and just loop your own step.
extern uint64_t nes_step_frame(void);

// Helpful PPU status (adjust/omit if not exported in your build)
extern uint64_t ppu_frame_count(void);
extern bool     ppu_in_vblank(void);

extern uint8_t ppu_last_dma_page(void);
extern uint8_t ppu_oamaddr_peek(void);
extern uint8_t ppu_regs_oam_peek(int index);

// --- tiny helpers ---
static inline int argi(const char* a, const char* b) { return strcmp(a,b)==0; }
static inline const char* bstr(bool v) { return v ? "true" : "false"; }

static void dump_dma_snapshot(void) {
    uint8_t page = ppu_last_dma_page();
    uint16_t base = ((uint16_t)page) << 8;
    uint8_t oamaddr = ppu_oamaddr_peek();

    fprintf(stderr, "[DMA SNAP] page=%02X base=$%04X oamaddr_start=%02X\n", page, base, oamaddr);

    fprintf(stderr, "  src[0..15]: ");
    for (int i = 0; i < 16; ++i) fprintf(stderr, "%02X ", cpu_read((uint16_t)(base + i)));
    fprintf(stderr, "\n");

    fprintf(stderr, "  OAM[0..15]: ");
    for (int i = 0; i < 16; ++i) fprintf(stderr, "%02X ", ppu_regs_oam_peek(i));
    fprintf(stderr, "\n");

    // Also peek the first sprite as (y,tile,attr,x)
    uint8_t y = ppu_regs_oam_peek(0);
    uint8_t t = ppu_regs_oam_peek(1);
    uint8_t a = ppu_regs_oam_peek(2);
    uint8_t x = ppu_regs_oam_peek(3);
    fprintf(stderr, "  OAM0: y=%3u tile=%02X attr=%02X x=%3u\n", y,t,a,x);
}




static void dump_oam_summary(const char* tag) {
    int nonzero = 0, xgt8 = 0;
    fprintf(stderr, "[OAM %s] first 8 sprites:\n", tag);
    for (int i = 0; i < 32; i += 4) {
        uint8_t y = ppu_regs_oam_peek(i+0);
        uint8_t t = ppu_regs_oam_peek(i+1);
        uint8_t a = ppu_regs_oam_peek(i+2);
        uint8_t x = ppu_regs_oam_peek(i+3);
        fprintf(stderr, "  #%d: y=%3u tile=%02X attr=%02X x=%3u\n", i/4, y, t, a, x);
    }
    for (int i = 0; i < 256; i += 4) {
        uint8_t y = ppu_regs_oam_peek(i+0);
        uint8_t t = ppu_regs_oam_peek(i+1);
        uint8_t a = ppu_regs_oam_peek(i+2);
        uint8_t x = ppu_regs_oam_peek(i+3);
        if (y|t|a|x) nonzero++;
        if (x > 8)    xgt8++;
    }
    fprintf(stderr, "[OAM %s] nonzero sprites=%d, sprites with x>8=%d\n", tag, nonzero, xgt8);
}



int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.nes> [--frames N] [--budget-frames M]\n", argv[0]);
        return 2;
    }
    const char* rom_path = argv[1];
    int frames_to_run = 2;
    int budget_frames  = 10;

    for (int i = 2; i < argc; ++i) {
        if (argi(argv[i], "--frames") && i+1 < argc)        frames_to_run = atoi(argv[++i]);
        else if (argi(argv[i], "--budget-frames") && i+1<argc) budget_frames = atoi(argv[++i]);
        else fprintf(stderr, "Unknown arg: %s\n", argv[i]);
    }

    // --- Load ROM ---
    size_t cart_size = 0;
    uint8_t* cart_data = ines_read_file(rom_path, &cart_size);
    if (!cart_data) { fprintf(stderr, "Failed to read ROM: %s\n", rom_path); return 1; }
    if (!ines_load(cart_data, cart_size)) { fprintf(stderr, "ines_load failed for %s\n", rom_path); free(cart_data); return 1; }
    free(cart_data);

    // --- Reset core (adjust to your API if names differ) ---
    bus_reset();
    cpu_reset();
    ppu_reset();
    apu_reset();

    int ok = 1;
    for (int f = 0; f < frames_to_run; ++f) {
        uint64_t start_frames = ppu_frame_count();
        uint64_t target       = start_frames + 1;

        // “Budget” here is just a guard to avoid infinite loops if a frame never finishes.
        // Since nes_step_frame() already advances to the next vblank, call it up to budget_frames times.
        int budget = budget_frames;
        while (ppu_frame_count() < target && budget-- > 0) {
            uint16_t pc_before = cpu_get_pc();
            bool vbl_before    = ppu_in_vblank();

            // Advance one frame (per your core)
            nes_step_frame();

            uint16_t pc_after  = cpu_get_pc();
            bool vbl_after     = ppu_in_vblank();

            // fprintf(stderr,
            //         "[TRACE] step_frame: PC %04X -> %04X | vblank %s -> %s | frames=%llu\n",
            //         (unsigned)pc_before, (unsigned)pc_after,
            //         bstr(vbl_before), bstr(vbl_after),
            //         (unsigned long long)ppu_frame_count());

            // fprintf(stderr, "[OK] Completed frame %d (frames=%llu, vblank=%s)\n",
            // f+1, (unsigned long long)ppu_frame_count(), bstr(ppu_in_vblank()));

            fprintf(stderr,
    "[STATS] dma=%d  w4014=%d  w2003=%d  w2004=%d  nmi=%d  spritebuf_w=%d  PPUCTRL=%02X PPUSTATUS=%02X\n",
    ppu_dma_count(),
    bus_io_4014_write_count(),
    ppu_oamaddr_write_count(),
    ppu_oamdata_write_count(),
    ppu_nmi_count(),
    bus_wram_spritebuf_write_count(),
    ppu_ppuctrl_get(),
    ppu_ppustatus_get());
            int last_dma = 0;

            // ... inside your per-frame loop, right after the [STATS] line:
            int now_dma = ppu_dma_count();
            if (now_dma > last_dma) {
                dump_dma_snapshot();
                last_dma = now_dma;
            }


            //dump_oam_summary("after frame");


        }

        if (ppu_frame_count() < target) {
            fprintf(stderr,
                "[WATCHDOG] No new frame within budget. frames=%llu vblank=%s PC=%04X\n",
                (unsigned long long)ppu_frame_count(),
                bstr(ppu_in_vblank()),
                (unsigned)cpu_get_pc());
            ok = 0;
            break;
        }

        fprintf(stderr, "[OK] Completed frame %d (frames=%llu, vblank=%s)\n",
                f+1, (unsigned long long)ppu_frame_count(), bstr(ppu_in_vblank()));
    }

    return ok ? 0 : 3;
}
