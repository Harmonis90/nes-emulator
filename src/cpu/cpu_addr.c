// 6502 addressing & operand fetch helpers for opcode implementations.
// - Simple modes return a 16-bit effective address.
// - Modes that can incur a page-cross cycle penalty return eff_t {addr,crossed}.
#include <stdint.h>

#include "cpu.h"
#include "bus.h"
#include "cpu_internal.h"
#include "cpu_addr.h"

// ------------------------------------------------------------
// Public fetch wrappers (advance PC)
// ------------------------------------------------------------
uint8_t cpu_fetch8(void) { return fetch8(); }
uint16_t cpu_fetch16(void) { return fetch16(); }

// ------------------------------------------------------------
// Small utilities
// ------------------------------------------------------------
static inline uint8_t rd8(uint16_t a) { return cpu_read(a); }

static inline uint8_t zp_rd(uint8_t zp_addr) { return rd8((uint16_t)zp_addr); }

static inline uint16_t zp_ptr16(uint8_t zp_base)
{
    // Zero-page 16-bit pointer, wraps within $0000-$00FF
    uint8_t lo = zp_rd(zp_base);
    uint8_t hi = zp_rd((uint8_t)(zp_base + 1)); // implicit wrap
    return (uint16_t)((uint16_t)hi << 8 | lo);
}

static inline uint16_t page_wrap_bug(uint16_t base)
{
    // 6502 JMP (indirect) bug: high byte wraps within the page
    uint16_t lo_addr = base;
    uint16_t hi_addr = (uint16_t)((base & 0xFF00) | ((base + 1) & 0x00FF));
    uint8_t lo = rd8(lo_addr);
    uint8_t hi = rd8(hi_addr);
    return (uint16_t)((uint16_t)hi << 8 | lo);
}

static inline uint8_t page_crossed(uint16_t a, uint16_t b){ return (uint8_t)(((a ^ b) & 0xFF00) != 0); }

// ------------------------------------------------------------
// Addressing modes that return a plain 16-bit address
// ------------------------------------------------------------
uint16_t cpu_addr_imm(void)
{
    // Return address of the immediate byte, but advance PC by 1.
    uint16_t pc = cpu_get_pc();
    cpu_set_pc((uint16_t)(pc + 1));
    return pc;
}

uint16_t cpu_addr_zp(void)
{
    uint8_t zp = cpu_fetch8();
    return (uint16_t)zp; // zero-page address $00nn --00 .. --FF
}

uint16_t cpu_addr_zpx(void)
{
    uint8_t zp = cpu_fetch8();
    zp = (uint8_t)(zp + cpu_get_x()); // wrap within zero-page
    return (uint16_t)zp;
}

uint16_t cpu_addr_zpy(void)
{
    uint8_t zp = cpu_fetch8();
    zp = (uint8_t)(zp + cpu_get_y()); // wrap within zero-page
    return (uint16_t)zp;
}

uint16_t cpu_addr_abs(void)
{
    return cpu_fetch16(); // little-endian immediate 16-bit
}

uint16_t cpu_addr_inx(void)
{
    // (Indirect,X): take ZP operand d, add X (wrap in ZP), deref 16-bit pointer
    uint8_t d = cpu_fetch8();
    uint8_t idx = (uint8_t)(d + cpu_get_x());
    return zp_ptr16(idx);
}

uint16_t cpu_addr_ind(void)
{
    // (Indirect): for JMP only, with page-wrap bug
    uint16_t base = cpu_fetch16();
    return page_wrap_bug(base);
}

// ------------------------------------------------------------
// Addressing modes that may cross a page boundary (return eff_t)
// ------------------------------------------------------------
eff_t cpu_addr_abx(void)
{
    eff_t e;
    uint16_t base = cpu_fetch16();
    e.addr = (uint16_t)(base + cpu_get_x());
    e.crossed = page_crossed(base, e.addr);
    return e;
}

eff_t cpu_addr_aby(void)
{
    eff_t e;
    uint16_t base = cpu_fetch16();
    e.addr = (uint16_t)(base + cpu_get_y());
    e.crossed = page_crossed(base, e.addr);
    return e;
}

eff_t cpu_addr_iny(void)
{
    eff_t e;
    uint8_t d = cpu_fetch8();
    uint16_t base = zp_ptr16(d);
    e.addr = (uint16_t)(base + cpu_get_y());
    e.crossed = page_crossed(base, e.addr);
    return e;
}