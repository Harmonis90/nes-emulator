#include <stdio.h>
#include <stdint.h>

#include "ppu.h"        // public shim
#include "ppu_regs.h"   // for vblank poke (optional)
#include "ppu_mem.h"    // for mirroring init (optional)

// helper: do a $2007 read (PPUDATA) after pointing PPUADDR ($2006)
static uint8_t ppu_read_at(uint16_t paddr) {
    // write $2006 high, then low -> sets v = paddr
    ppu_write(0x2006, (uint8_t)((paddr >> 8) & 0x3F)); // upper 6 bits only
    ppu_write(0x2006, (uint8_t)(paddr & 0xFF));
    return ppu_read(0x2007);
}

static void ppu_write_at(uint16_t paddr, uint8_t v) {
    ppu_write(0x2006, (uint8_t)((paddr >> 8) & 0x3F));
    ppu_write(0x2006, (uint8_t)(paddr & 0xFF));
    ppu_write(0x2007, v);
}

// buffered read helper for non-palette: do a dummy read, then the real one
static uint8_t ppu_read_at_nametable(uint16_t paddr) {
    // set v = paddr
    ppu_write(0x2006, (uint8_t)((paddr >> 8) & 0x3F));
    ppu_write(0x2006, (uint8_t)(paddr & 0xFF));
    (void)ppu_read(0x2007);             // dummy (fills buffer)
    // re-point (optional if PPUCTRL inc=1 and you didn’t advance)
    paddr &= 0x3FFF;
    ppu_write(0x2006, (uint8_t)((paddr >> 8) & 0x3F));
    ppu_write(0x2006, (uint8_t)(paddr & 0xFF));
    return ppu_read(0x2007);            // real value
}

int main(void) {
    // 1) Reset + choose nametable mirroring for predictable results
    ppu_reset();
    ppu_mem_set_mirroring(MIRROR_HORIZONTAL); // or VERTICAL; either is fine here

    // 2) Verify PPUDATA buffered reads for non-palette addresses
    //    First read returns old buffer (power-on 0), second returns the actual byte.
    //    We'll write at $2000 (nametable) and read it back.
    ppu_write_at(0x2000, 0x5A);

    // Set increment by 1 (PPUCTRL bit2 = 0) so reads don't skip rows
    ppu_write(0x2000, 0x00);

    // Read #1 at $2000 -> should return OLD buffer (0x00 at power-on)
    uint8_t r1 = ppu_read_at(0x2000);
    // Read #2 at $2001 (because the first read incremented v) -> returns the byte at $2001's old buffer;
    // to get the byte we wrote at $2000, reset v and read again:
    uint8_t r2 = ppu_read_at(0x2000);

    printf("Buffered read test: first=%02X (expect 00-ish), second=%02X (expect 5A)\n", r1, r2);

    // 3) Verify PALETTE reads are NOT buffered (direct)
    //    Write palette entry $3F00 = 0x3C, and its alias at $3F10 should mirror to $3F00
    ppu_write_at(0x3F00, 0x3C);
    uint8_t p1 = ppu_read_at(0x3F00); // direct (no buffer)
    uint8_t p2 = ppu_read_at(0x3F10); // should mirror $3F00
    printf("Palette direct read: $3F00=%02X (expect 3C), alias $3F10=%02X (expect 3C)\n", p1, p2);

    // 4) Verify $2000 bit2 changes VRAM increment (1 vs 32)
    //    Write two bytes to $2007 with inc=1, then two with inc=32 and see how the address advances.
    //    (We’ll just make sure it doesn’t crash and increments; deep check needs a tiny tracer.)
    ppu_write(0x2000, 0x00); // inc = 1
    ppu_write_at(0x2000, 0x11);
    ppu_write(0x2000, 0x04); // inc = 32
    ppu_write_at(0x2000, 0x22);
    printf("VRAM increment paths exercised (inc=1 then inc=32).\n");

    // 5) Basic OAM poke/readback via $2003/$2004 (register path is simple)
    ppu_write(0x2003, 0x00);     // OAMADDR = 0
    ppu_write(0x2004, 0xAB);     // write one byte
    ppu_write(0x2003, 0x00);     // reset address to 0
    uint8_t oam0 = ppu_read(0x2004);
    printf("OAM test: OAM[0]=%02X (expect AB)\n", oam0);

    // 6) Optional: fake vblank raise/clear to test $2002 behavior
    ppu_regs_set_vblank(true);
    uint8_t s1 = ppu_read(0x2002); // read PPUSTATUS, should have bit7 set then clear it
    uint8_t s2 = ppu_read(0x2002); // immediate next read should show bit7 cleared
    printf("PPUSTATUS read: first=%02X (VBlank set), second=%02X (VBlank cleared)\n", s1, s2);

    // A) Nametable mirroring sanity (H/V)
    // HORIZONTAL: [A A B B]  -> NT1 mirrors NT0
    ppu_mem_set_mirroring(MIRROR_HORIZONTAL);
    ppu_write_at(0x2000, 0x11);                  // NT0
    uint8_t mh = ppu_read_at_nametable(0x2400);  // NT1 (mirror of NT0 in H)
    printf("H-mirror NT1==NT0: %02X (expect 11)\n", mh);

    // VERTICAL: [A B A B] -> NT3 mirrors NT1
    ppu_mem_set_mirroring(MIRROR_VERTICAL);
    ppu_write_at(0x2400, 0x22);                  // NT1
    uint8_t mv = ppu_read_at_nametable(0x2C00);  // NT3 (mirror of NT1 in V)
    printf("V-mirror NT3==NT1: %02X (expect 22)\n", mv);

    // B) Palette alias the other way: write $3F10, read $3F00
    ppu_write_at(0x3F10, 0x4D);
    uint8_t pal_alias = ppu_read_at(0x3F00);
    printf("Palette alias $3F10->$3F00: %02X (expect 4D)\n", pal_alias);

    // C) Loopy scroll writes: ensure x,fineY,coarseY land in t and affect v after $2006
    // Write PPUSCROLL x=5 (fine X) and coarse X=3
    ppu_write(0x2005, (uint8_t)((3 << 3) | 5));
    // Write PPUSCROLL fineY=6, coarseY=7
    ppu_write(0x2005, (uint8_t)((6 & 7) | (7 << 3)));
    // Now set PPUADDR high/low = copy t->v
    ppu_write(0x2006, 0x20); // hi (masked to 14 bits)
    ppu_write(0x2006, 0x00); // lo
    // A simple smoke: write then read at v; increment should follow PPUCTRL bit2
    ppu_write(0x2000, 0x00); // inc=1
    ppu_write(0x2007, 0xAA);
    uint8_t sc_smoke = ppu_read(0x2007); // buffered read at v+1
    printf("Scroll smoke OK (no crash, buffered read=%02X)\n", sc_smoke);

    // D) NMI enable behavior: set PPUCTRL bit7 then raise vblank -> should call cpu_nmi()
    // (You’ll “see” this once CPU IRQ/NMI plumbing is hooked up. For now it just shouldn’t crash.)
    ppu_write(0x2000, 0x80); // NMI enable
    ppu_regs_set_vblank(true);

    return 0;
}
