#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "bus.h"
#include "cpu_internal.h"
#include "cpu_table.h"
#include "cpu_ops.h"

// -----------------------------------------------------------------------------
// Internal CPU state
// -----------------------------------------------------------------------------
static struct
{
    uint64_t cycles; // total cycles since reset
    int irq_pending; // (legacy) maskable irq request - no longer used by MMC3 path
    int nmi_pending; // non-maskable IRQ requested
} cpu_ctx;

// Level-sensitive IRQ line (0=inactive, 1=asserted). Mappers (e.g. MMC3)
// assert this line; CPU services it when I=0. It stays asserted until cleared.
static int irq_line = 0;

// -----------------------------------------------------------------------------
// Cycle counter API
// -----------------------------------------------------------------------------
uint64_t cpu_get_cycles(void)
{
    return cpu_ctx.cycles;
}

void cpu_cycles_add(int n)
{
    cpu_ctx.cycles += (uint64_t)n;
}

// -----------------------------------------------------------------------------
// Reset / IRQ / NMI
// -----------------------------------------------------------------------------
void cpu_reset(void)
{
    memset(&cpu_ctx, 0, sizeof(cpu_ctx));
    irq_line = 0;

    // Reset registers
    cpu_set_a(0);
    cpu_set_x(0);
    cpu_set_y(0);
    cpu_set_p(FLAG_U | FLAG_I); // IRQ disabled after reset
    cpu_set_sp(0xFD);

    // Load reset vector at $FFFC/$FFFD
    uint8_t lo =  cpu_read(0xFFFC);
    uint8_t hi =  cpu_read(0xFFFD);
    cpu_set_pc((uint16_t)(lo | (hi << 8)));

    cpu_ctx.cycles = 7; // reset takes 7 cycles
}

// New: level IRQ API (assert/clear). Keep cpu_irq() as a compatibility alias.
void cpu_irq_assert(void) { irq_line = 1; }
void cpu_irq_clear(void)  { irq_line = 0; }
void cpu_irq(void)        { cpu_irq_assert(); }

void cpu_nmi(void)
{
    cpu_ctx.nmi_pending = 1;
}

// -----------------------------------------------------------------------------
// One instruction step
// -----------------------------------------------------------------------------
void cpu_step(void)
{
    // Service NMI edge if latched
    if (cpu_ctx.nmi_pending) {
        cpu_ctx.nmi_pending = 0;
        interrupt_enter(0xFFFA, 0);
        cpu_cycles_add(7);
    }

    // Service IRQ on level if asserted and I=0 (do NOT clear irq_line here;
    // mapper must acknowledge/clear via cpu_irq_clear()).
    if (irq_line && !get_flag(FLAG_I)) {
        interrupt_enter(0xFFFE, 0);
        cpu_cycles_add(7);
        // irq_line remains asserted until mapper clears it (e.g., MMC3 $E000)
    }

    const uint64_t cyc0 = cpu_get_cycles();
    const uint16_t pc   = cpu_get_pc();
    const uint8_t  op   = cpu_read(pc);

    // trace BEFORE executing the opcode
    TRACE("PRE  C=%llu  PC=%04X  OP=%02X  A=%02X X=%02X Y=%02X P=%02X SP=%02X\n",
          (unsigned long long)cyc0, pc, op,
          cpu_get_a(), cpu_get_x(), cpu_get_y(), cpu_get_p(), cpu_get_sp());

    // base cycles + bump PC past opcode
    cpu_cycles_add(cpu_base_cycles[op]);
    cpu_set_pc((uint16_t)(pc + 1));

    // execute
    cpu_dispatch[op]();

    // trace AFTER executing the opcode
    const uint64_t cyc1 = cpu_get_cycles();
    TRACE("POST C=%llu (+%llu) PC=%04X  A=%02X X=%02X Y=%02X P=%02X\n",
          (unsigned long long)cyc1, (unsigned long long)(cyc1 - cyc0),
          cpu_get_pc(), cpu_get_a(), cpu_get_x(), cpu_get_y(), cpu_get_p());
}

// -----------------------------------------------------------------------------
// Optional disassembler (for debugging / testing)
// -----------------------------------------------------------------------------
const char* cpu_dissasm(uint16_t pc, char* buf, size_t buflen)
{
    if (!buf || buflen == 0) { return NULL; }

    uint8_t opcode = cpu_read(pc);
    const char* mnem = cpu_mnemonic[opcode];
    const char* am   = cpu_addrmode[opcode];
    uint8_t len      = cpu_instr_len[opcode];

    if (len == 1) {
        snprintf(buf, buflen, "%04X  %02X       %s %s",
                 pc, opcode, mnem, am);
    } else if (len == 2) {
        uint8_t op1 = cpu_read(pc + 1);
        snprintf(buf, buflen, "%04X  %02X %02X    %s %s",
                 pc, opcode, op1, mnem, am);
    } else if (len == 3) {
        uint8_t op1 = cpu_read(pc + 1);
        uint8_t op2 = cpu_read(pc + 2);
        snprintf(buf, buflen, "%04X  %02X %02X %02X %s %s",
                 pc, opcode, op1, op2, mnem, am);
    } else {
        snprintf(buf, buflen, "%04X  %02X       %s %s",
                 pc, opcode, mnem, am);
    }

    return buf;
}
