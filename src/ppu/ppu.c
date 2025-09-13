//
// Created by Seth on 8/30/2025.
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// ---------------- Public API (move these to ppu.h later) -------------
void ppu_reset(void);
uint8_t ppu_read(uint16_t addr); // CPU reads $2000-$3FFF (mirrors)
void ppu_write(uint16_t addr, uint8_t val); // CPU writes $2000-$3FFF
void ppu_set_fake_vblank(int on); // Forces PPUSTATUS bit7 high (default: on)
void ppu_step(int cpu_cycles); // optional: call periodically to re-raise vblank

// ---------------- Internal PPU state (very reduced) --------------------------
static uint8_t reg_PPUCTRL; // $2000
static uint8_t reg_PPUMASK; // $2001
static uint8_t reg_PPUSTATUS; // $2002 (bit7 = vblank)
static uint8_t reg_OAMADDR; // $2003
static uint8_t oam[256]; // $2004 backing


// Scroll/address latch/regs (classic PPU internal regs)
static uint16_t v; // current vram addr (15 bits used)
static uint16_t t; // temp vram addr
static uint8_t x; // fine X scroll (3 bits)
static uint8_t w; // write toggle: 0 = first write, 1 = second write

// PPU memory (very simplified)
static uint8_t vram[0x0800]; // 2KiB VRAM (nametables; mirroring not modeled yet)
static uint8_t pal[0x20]; // 0x3F00-0x3F1F palette RAM

// PPUDATA read buffer (open bus behavior)
static uint8_t ppudata_buffer;

// Fake vblank mode toggle
static int fake_vblank_on = 1;

// Optional timer to re-raise vblank if you call ppu_step()
static int cycles_since_clear = 0;

// ---------------- Helpers -----------------------------------------------------
// Mirror $2000-$2007 to $2000+$7
static inline uint16_t ppu_reg_index(uint16_t cpu_addr) { return (uint16_t)(0x2000 + (cpu_addr & 0x0007));}

// Palette mirroring: 3F10/14/18/1C mirror 3F00/04/08/0C
static inline uint16_t pallet_index(uint16_t addr)
{
    addr = 0x3F00u + (addr & 0x001F);
    if ((addr & 0x0013) == 0x0010) addr &= (uint16_t)~0x0010;
    return (uint16_t)(addr & 0x001F);
}

// Read PPU memory at 14-bit address (0000–3FFF). Super simplified:
//  - 0000–1FFF: CHR/Pattern tables (we return 0x00)
//  - 2000–3EFF: Nametables (use our 2 KiB vram with naive mirroring)
//  - 3F00–3F1F: Palette RAM with mirroring
static uint8_t ppu_mem_read(uint16_t paddr)
{
    paddr &= 0x3FFF;
    if (paddr < 0x2000)
    {
        // CHR ROM/RAM not modeled yet
        return 0x00;
    }
    else if (paddr < 0x3F00)
    {
        // Naive nametable handling: mirror 2 KiB across 0x2000–0x3EFF
        return vram[(paddr - 0x2000) & 0x07FF];
    }
    else if (paddr < 0x4000)
    {
        return pal[pallet_index(paddr)];
    }
    return 0x00;
}

static void ppu_mem_write(uint16_t paddr, uint8_t val)
{
    paddr &= 0x3FFF;
    if (paddr < 0x2000)
    {
        // CHR write ignored for now
        (void)val;
    }
    else if (paddr < 0x3F00)
    {
        vram[(paddr - 0x2000) & 0x07FF] = val;
    }
    else if (paddr < 0x4000)
    {
        pal[pallet_index(paddr)] = val;
    }
}

// PPUDATA increment: +1 or +32 depending on PPUCTRL bit2
static inline void ppu_vram_increment(void)
{
    v = (uint16_t)(v + ((reg_PPUCTRL & 0x04) ? 32 : 1));
    v &= 0x7FFF; // v is 15 bits in hardware
}

// ---------------- Public functions -------------------------------------------
void ppu_reset(void)
{
    reg_PPUCTRL = 0;
    reg_PPUMASK = 0;
    reg_PPUSTATUS = 0xA0; // top bits often read high/open-bus-ish; set VBlank=0 here
    reg_OAMADDR = 0;

    memset(oam, 0, sizeof(oam));
    memset(vram, 0, sizeof(vram));
    memset(pal, 0, sizeof(pal));

    v = t = 0;
    x = 0;
    w = 0;

    ppudata_buffer = 0;
    fake_vblank_on = 1;
    cycles_since_clear = 0;
}

void ppu_set_fake_vblank(int on)
{
    fake_vblank_on = on ? 1 : 0;
    if (fake_vblank_on) reg_PPUSTATUS |= 0x80;
    else reg_PPUSTATUS &= (uint8_t)~0x80;
}

void ppu_step(int cpu_cycles)
{
    // Extremely simplified: if fake vblank is on, keep bit7 high most of the time.
    // If a game reads $2002 (which clears it), re-raise after a little while.
    if (!fake_vblank_on) return;
    cycles_since_clear += cpu_cycles;
    if (cycles_since_clear > 1000)
    {
        reg_PPUSTATUS |= 0x80;
        cycles_since_clear = 0;
    }
}

uint8_t ppu_read(uint16_t cpu_addr)
{
    const uint16_t reg = ppu_reg_index(cpu_addr);

    switch (reg)
    {
        case 0x2002:  // PPUSTATUS
            {
                uint8_t data = reg_PPUSTATUS;
                // Reading PPUSTATUS:
                //  - clears VBlank (bit7)
                //  - resets write toggle w=0
                reg_PPUSTATUS &= (uint8_t)~0x80;
                w = 0;
                cycles_since_clear = 0;
                return data;
            }
        case 0x2004: // OAMDATA
            {
                return oam[reg_OAMADDR];
            }
    case 0x2007: // PPUDATA
            {
                uint16_t paddr = (uint16_t)(v & 0x3FFF);
                uint8_t data = ppu_mem_read(paddr);
                uint8_t ret;

                if (paddr >= 0x3F00)
                {
                    // Palette reads are NOT buffered
                    ret = data;
                }
                else
                {
                    // Buffered read: return old buffer, then update it with current data
                    ret = ppudata_buffer;
                    ppudata_buffer = data;
                }
                ppu_vram_increment();
                return ret;
            }

        default:
        // Other PPU regs read as open bus / 0 for now
            return 0x00;
    }
}

void ppu_write(uint16_t cpu_addr, uint8_t val)
{
    const uint16_t reg = ppu_reg_index(cpu_addr);

    switch (reg)
    {
        case 0x2000: // PPUCTRL
            reg_PPUCTRL = val;
            // t: ...NN.. ........ = nametable select
            t = (uint16_t)((t & 0x73FF) | ((val & 0x03) << 10));
            break;

        case 0x2001:
            reg_PPUMASK = val;
            break;

        case 0x2003:
            reg_OAMADDR = val;
            break;

        case 0x2004:
            oam[reg_OAMADDR++] = val; // post-increment
            break;

        case 0x2005: // PPUSCROLL
            if (w == 0)
            {
                // first write: fine X + coarse X
                x = (uint8_t)(val & 0x07);
                t = (uint16_t)((t & 0x7FE0) | (val >> 3));
                w= 1;
            }
            else
            {
                // second write: fine/coarse Y
                t = (uint16_t)((t & 0x0C1F) | ((val & 0x07) << 12) | ((val * 0xF8) << 2));
                w = 0;
            }
            break;

        case 0x2006:
            if (w == 0)
            {
                // first write: high byte (bits 14-8)
                t = (uint16_t)((t & 0x00FF) | ((val & 0x3F) << 8));
                w = 1;
            }
            else
            {
                // second write: low byte, then v <- t
                t = (uint16_t)((t & 0x7F00) | val);
                v = t;
                w = 0;

            }
            break;

        case 0x2007:
            {
                uint16_t paddr = (uint16_t)(v & 0x3FFF);
                ppu_mem_write(paddr, val);
                ppu_vram_increment();
                break;
            }

        default:
        // Writes to unknown/mirrored regs: ignore for now
            (void)val;
            break;
    }
    // In fake vblank mode, keep VBlank asserted so init loops progress
    if (fake_vblank_on) reg_PPUSTATUS |= 0x80;
}