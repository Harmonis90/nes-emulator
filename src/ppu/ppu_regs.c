#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "bus.h"
#include "cpu.h"
#include "ppu_mem.h"
#include "ppu_regs.h"

#include "ppu.h"


// -------------------------- Internal PPU state --------------------------------
// CPU-visible registers (latched values)
static uint8_t PPUCTRL = 0x00; // $2000
static uint8_t PPUMASK = 0x00; // $2001
static uint8_t PPUSTATUS = 0x00; // $2002 (bit7=VBlank, bit6=Sprite0 hit, bit5=Sprite overflow)
static uint8_t OAMADDR = 0x00; // $2003
static uint8_t OAMDATA = 0x00; // $2004 (through buffer)

// Loopy scrolling registers
// v = current VRAM address (15-bit)
// t = temporary VRAM address (15-bit)
// x = fine X scroll (3-bit)
// w = write toggle for $2005/$2006 (0 or 1)
static uint16_t v = 0x0000;
static uint16_t t = 0x0000;
static uint8_t x = 0; // fine X (0..7)
static uint8_t w = 0;  // 0 = first write, 1 = second write

// $2007 read buffer (one-instruction-late for non-palette reads)
static uint8_t vram_read_buffer = 0x00;

// Tiny OAM (256 bytes)
static uint8_t OAM[256];

// -------------------------- Helpers -------------------------------------------
static inline uint16_t vram_increment(void)
{
    // PPUCTRL bit 2: 0 => increment by 1; 1 => increment by 32
    return (PPUCTRL & 0x04) ? 32 : 1;
}

static inline uint16_t clamp_14bit(uint16_t a)
{
    return (uint16_t)(a & 0x3FFF);
}

// -------------------------- Public API ----------------------------------------
void ppu_regs_reset(void)
{
    PPUCTRL = 0x00;
    PPUMASK = 0x00;
    PPUSTATUS = 0x00;
    OAMADDR = 0x00;
    OAMDATA = 0x00;

    v = t = 0x0000;
    x = 0;
    w = 0;

    vram_read_buffer = 0x00;
    memset(OAM, 0, sizeof(OAM));
}

void ppu_regs_oam_clear(void) { memset(OAM, 0, sizeof(OAM)); }
uint8_t ppu_regs_oam_peek(uint8_t index) { return OAM[index]; }
void ppu_regs_oam_poke(uint8_t index, uint8_t v_) { OAM[index] = v_; }

void ppu_oam_dma(uint8_t page)
{
    // Copy 256 bytes from CPU page (page << 8) to OAM starting at OAMADDR.
    // This matches your OAM behavior: $2004 write auto-increments OAMADDR.
    uint16_t base = (uint16_t)((uint16_t)page << 8);
    uint8_t addr = OAMADDR;

    for (int i = 0; i < 256; i++)
    {
        uint8_t v = cpu_read((uint16_t)(base + i));
        OAM[addr] = v;
        addr++;
    }
    OAMADDR = addr;
}


// Set/clear VBlank bit, optionally trigger NMI if enabled
void ppu_regs_set_vblank(bool on)
{
    if (on)
    {
        PPUSTATUS |= 0x80;
        if (PPUCTRL & 0x80)
        {
            // Generate NMI on rising edge of VBlank when bit 7 is set
            cpu_nmi();
        }
    }
    else
    {
        PPUSTATUS &= ~0x80;
    }
}

// -------------------------- Register I/O --------------------------------------
// CPU reads $2000-$2007 (mirrored every 8 up to $3FFF)
uint8_t ppu_regs_read(uint16_t addr)
{
    uint16_t r = (uint16_t)(0x2000 + (addr & 0x0007));

    switch (r)
    {
        case 0x2000: // PPUCTRL (write-only on hardware; read returns bus content)
            return PPUCTRL; // acceptable for a dev emulator; HW returns open bus

        case 0x2001: // PPUMASK (write-only)
            return PPUMASK;

        case 0x2002: // PPUSTATUS
            {
                // Reading clears VBlank (bit7), resets write toggle w
                uint8_t val = PPUSTATUS;
                PPUSTATUS &= (uint8_t)~0x80;
                w = 0;
                return val;
            }

        case 0x2003: // OAMADDR
            return OAMADDR;

        case 0x2004: // OAMDATA (read data at OAMADDR, no increment on read)
            return OAM[OAMADDR];

        case 0x2005: // PPUSCROLL (write-only)
            return 0x00;

        case 0x2006: // PPUADDR (write-only)
            return 0x00;

        case 0x2007: // PPUDATA
            {
                uint16_t addr_v = clamp_14bit(v);
                uint8_t data;

                if ((addr_v & 0x3F00) == 0x3F00)
                {
                    // Palette reads are not buffered; they return direct value
                    data = ppu_mem_read(addr_v);
                    v = clamp_14bit(addr_v + vram_increment());
                    // Still update the internal buffer with the mirror underlying value
                    vram_read_buffer = ppu_mem_read((uint16_t)(addr_v & 0x2FFF));
                    return data;
                }
                else
                {
                    // Return the buffered value, then refill buffer from current addr
                    data = vram_read_buffer;
                    vram_read_buffer = ppu_mem_read(addr_v);
                    v = clamp_14bit(addr_v + vram_increment());
                    return data;
                }
            }

    }
    return 0x00;
}

// CPU writes $2000-$2007 (mirrored every 8 up to $3FFF)
void ppu_regs_write(uint16_t addr, uint8_t data)
{
    uint16_t r = (uint16_t)(0x2000 + (addr & 0x0007));

    switch (r)
    {
        case 0x2000: // PPUCTRL
            PPUCTRL = data;
            // t: ...AB.. ........ = nametable select from PPUCTRL bits 0-1
            t = (uint16_t)((t & 0xF3FF) | ((data & 0x03) << 10));
            return;

        case 0x2001: // PPUMASK
            PPUMASK = data;
            return;

        case 0x2002: // PPUSTATUS (write ignored in HW; some bits latch open bus). Ignore.
            return;

        case 0x2003: // OAMADDR
            OAMADDR = data;
            return;

        case 0x2004: // OAMDATA
            OAM[OAMADDR++] = data; // HW increments after write
            return;

        case 0x2005: // PPUSCROLL
            if (w == 0)
            {
                // First write: x scroll (coarse X & fine X)
                // t: ..... ...ABCDE = coarse X (data >> 3)
                // x: fine X = data & 0x07
                x = (uint8_t)(data & 0x07);
                t = (uint16_t)((t & 0xFFE0) | (data >> 3));
                w = 1;
            }
            else
            {
                // Second write: y scroll (coarse Y & fine Y)
                // t: CBA.. ........ = fine Y (data & 0x07)
                // t: ..... ABCDE... = coarse Y (data >> 3)
                t = (uint16_t)((t & 0x8FFF) | ((data & 0x07) << 12));  // fine Y -> bits 12-14
                t = (uint16_t)((t & 0xFC1F) | ((data & 0xF8) << 2)); // coarse Y -> bits 5-9
                w = 0;
            }
            return;

        case 0x2006: // PPUADDR
            if (w == 0)
            {
                // First write: high 6 bits (mask to 14-bit)
                t = (uint16_t)((t & 0x00FF) | ((data & 0x3F) << 8));
                w = 1;
            }
            else
            {
                // Second write: low 8 bits; then copy to v
                t = (uint16_t)((t & 0xFF00) | data);
                v = t;
                w = 0;
            }
            return;

        case 0x2007:
            uint16_t addr_v = clamp_14bit(v);
            ppu_mem_write(addr_v, data);
            v = clamp_14bit(addr_v + vram_increment());
            return;
    }
}