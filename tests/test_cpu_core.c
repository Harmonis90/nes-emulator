// tests/test_cpu_core.c
// Unit tests for the 6502 CPU core using the mock bus.
//
// Build as a separate test target that links against your CPU sources:
//   - src/cpu/cpu.c
//   - src/cpu/cpu_table.c
//   - src/cpu/cpu_ops.c
//   - src/cpu/cpu_addr.c
//   - src/cpu/cpu_internal.c
// and this test directory (test_bus.c, test_cpu_core.c).
//
// Example CMake snippet:
//
// add_executable(cpu-core-tests
//     tests/test_bus.c
//     tests/test_cpu_core.c
// )
// target_link_libraries(cpu-core-tests PRIVATE nes-emulator-core)
// # IMPORTANT: Ensure nes-emulator-core does NOT also compile your real bus.c
// # for this test target, or you’ll have duplicate cpu_read/cpu_write symbols.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "cpu.h"
#include "test_bus.h"

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERT FAILED: %s (line %d)\n", msg, __LINE__); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ_U8(exp, got, what) do { \
    if ((uint8_t)(exp) != (uint8_t)(got)) { \
        fprintf(stderr, "ASSERT FAILED: %s expected %u got %u (line %d)\n", \
            what, (unsigned)(uint8_t)(exp), (unsigned)(uint8_t)(got), __LINE__); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ_U16(exp, got, what) do { \
    if ((uint16_t)(exp) != (uint16_t)(got)) { \
        fprintf(stderr, "ASSERT FAILED: %s expected 0x%04X got 0x%04X (line %d)\n", \
            what, (unsigned)(uint16_t)(exp), (unsigned)(uint16_t)(got), __LINE__); \
        return 1; \
    } \
} while (0)

#define ASSERT_FLAG(flagmask, set) do { \
    uint8_t p = cpu_get_p(); \
    int is_set = (p & (flagmask)) != 0; \
    if ((set) != is_set) { \
        fprintf(stderr, "ASSERT FAILED: flag 0x%02X expected %d got %d (line %d)\n", \
            (flagmask), (set), is_set, __LINE__); \
        return 1; \
    } \
} while (0)

// Small helpers
static void step_n(int n) { for (int i=0;i<n;i++) cpu_step(); }

// --- Tests ---

static int test_reset_vector_and_basic_load_store(void) {
    tb_reset_memory();

    // Program at $8000:
    // A9 10    LDA #$10           ; A = 0x10
    // AA       TAX                ; X = 0x10
    // E8       INX                ; X = 0x11
    // 69 05    ADC #$05           ; A = 0x15, C=0
    // 85 00    STA $00            ; [$0000] = 0x15
    // A5 00    LDA $00            ; A = 0x15
    // 00       BRK                ; (end)
    const uint8_t prog[] = {
        0xA9,0x10, 0xAA, 0xE8, 0x69,0x05, 0x85,0x00, 0xA5,0x00, 0x00
    };
    tb_load_program(0x8000, prog, sizeof(prog));
    tb_set_reset_vector(0x8000);

    cpu_reset();
    ASSERT_EQ_U16(0x8000, cpu_get_pc(), "PC after reset");

    uint64_t c0 = cpu_get_cycles();
    step_n(1); // LDA #$10
    ASSERT_EQ_U8(0x10, cpu_get_a(), "LDA imm A");
    ASSERT_FLAG(FLAG_Z, 0);
    ASSERT_FLAG(FLAG_N, 0);

    step_n(1); // TAX
    ASSERT_EQ_U8(0x10, cpu_get_x(), "TAX X");
    ASSERT_FLAG(FLAG_Z, 0);
    ASSERT_FLAG(FLAG_N, 0);

    step_n(1); // INX
    ASSERT_EQ_U8(0x11, cpu_get_x(), "INX X");

    step_n(1); // ADC #$05
    ASSERT_EQ_U8(0x15, cpu_get_a(), "ADC imm A");
    ASSERT_FLAG(FLAG_C, 0);
    ASSERT_FLAG(FLAG_V, 0);

    step_n(1); // STA $00
    ASSERT_EQ_U8(0x15, tb_peek(0x0000), "STA zp mem");

    step_n(1); // LDA $00
    ASSERT_EQ_U8(0x15, cpu_get_a(), "LDA zp A");

    step_n(1); // BRK
    uint64_t c1 = cpu_get_cycles();
    ASSERT_TRUE(c1 > c0, "cycle counter progressed");

    return 0;
}

static int test_page_cross_penalty_on_read_indexed(void) {
    tb_reset_memory();
    tb_set_reset_vector(0x9000);

    // Arrange: base at $90FE, Y=0x02 -> address $9100 (page cross)
    //  A0 02      LDY #$02
    //  B1 10      LDA ($10),Y    ; ZP $10/11 -> pointer to $90FE
    //  00         BRK
    // Set ZP $10/$11 to $FE $90
    tb_poke(0x0010, 0xFE);
    tb_poke(0x0011, 0x90);
    tb_poke(0x9100, 0x3C); // final read target after Y

    const uint8_t prog[] = {
        0xA0,0x02,
        0xB1,0x10,
        0x00
    };
    tb_load_program(0x9000, prog, sizeof(prog));

    cpu_reset();
    // Execute LDY #$02
    step_n(1);
    ASSERT_EQ_U8(0x02, cpu_get_y(), "LDY imm");

    // Measure cycles before LDA (ind),Y
    uint64_t before = cpu_get_cycles();
    step_n(1);
    uint64_t after = cpu_get_cycles();

    // Base cycles for LDA (ind),Y is 5, +1 if page crossed => expect 6 cycles increment here.
    ASSERT_EQ_U8(0x3C, cpu_get_a(), "LDA (ind),Y result");
    ASSERT_TRUE((after - before) >= 6, "page cross penalty applied");

    return 0;
}

static int test_branch_taken_and_page_cross(void) {
    tb_reset_memory();
    tb_set_reset_vector(0xA000);

    // Arrange:
    //  A9 00      LDA #$00     ; Z set
    //  F0 7D      BEQ +0x7D    ; taken, forward within page (adds +1 for taken, no cross)
    //  00         BRK          ; should be skipped
    //  ... target:
    //  A9 01      LDA #$01
    //  10 7D      BPL +0x7D    ; taken, and force a page-cross by placing target near boundary
    //  00         BRK          ; should be skipped
    //  [target after cross]
    //  00         BRK

    // Lay out code so second branch crosses a page:
    // Put first block at A000.., second branch target at A0FF + 0x7D -> A17C (crosses A1xx)
    const uint8_t prog[] = {
        0xA9,0x00,          // A000
        0xF0,0x7D,          // A002 (to A081)
        0x00,               // A004 (skipped)
        // A081:
        0xA9,0x01,          // A081
        0x10,0x7D,          // A083 (to A102) — ensure cross later
        0x00,               // A085 (skipped)
        // ... fill up to A102 with NOPs
    };
    tb_load_program(0xA000, prog, sizeof(prog));
    // Fill NOPs up to A102 and place final BRK
    for (uint16_t a = 0xA086; a < 0xA102; ++a) tb_poke(a, 0xEA);
    tb_poke(0xA102, 0x00); // BRK

    cpu_reset();

    // 1) LDA #$00
    step_n(1);
    ASSERT_FLAG(FLAG_Z, 1);

    // 2) BEQ taken (within page): +1 cycle for taken (maybe +1 more if cross, but not here)
    uint64_t c0 = cpu_get_cycles();
    step_n(1);
    uint64_t c1 = cpu_get_cycles();
    ASSERT_TRUE((c1 - c0) >= 3, "BEQ taken at least base(2)+1");

    // Landed at A081, execute LDA #$01
    step_n(1);
    ASSERT_EQ_U8(0x01, cpu_get_a(), "LDA post-branch");

    // 3) BPL taken and page-crosses: expect +1 taken, +1 cross
    uint64_t c2 = cpu_get_cycles();
    step_n(1);
    uint64_t c3 = cpu_get_cycles();
    ASSERT_TRUE((c3 - c2) >= 4, "BPL taken + page-cross penalty");

    // Final BRK executes
    step_n(1);

    return 0;
}

static int test_jsr_rts_stack(void) {
    tb_reset_memory();
    tb_set_reset_vector(0xB000);

    // Program:
    //  20 05 B0   JSR $B005
    //  00         BRK           ; should return here after RTS
    //  A9 42      LDA #$42      ; subroutine @ B005
    //  60         RTS
    const uint8_t prog[] = {
        0x20,0x05,0xB0,
        0x00,
        0xA9,0x42,
        0x60
    };
    tb_load_program(0xB000, prog, sizeof(prog));

    cpu_reset();
    uint8_t sp0 = cpu_get_sp();

    // JSR
    step_n(1);
    // LDA #$42 in subroutine
    step_n(1);
    ASSERT_EQ_U8(0x42, cpu_get_a(), "LDA in subroutine");
    // RTS
    step_n(1);
    // Back at BRK
    step_n(1);

    uint8_t sp1 = cpu_get_sp();
    ASSERT_EQ_U8(sp0, sp1, "SP restored after JSR/RTS");

    return 0;
}

static int test_nmi_irq_paths(void) {
    tb_reset_memory();
    tb_set_reset_vector(0xC000);
    tb_set_nmi_vector(0xC100);
    tb_set_irq_vector(0xC200);

    // Program at C000: NOP, NOP, BRK (should be replaced by interrupts before reaching BRK)
    tb_load_program(0xC000, (const uint8_t[]){ 0xEA, 0xEA, 0x00 }, 3);
    // NMI vector points to: LDA #$11, RTI
    tb_load_program(0xC100, (const uint8_t[]){ 0xA9,0x11, 0x40 }, 3);
    // IRQ vector points to: LDA #$22, RTI
    tb_load_program(0xC200, (const uint8_t[]){ 0xA9,0x22, 0x40 }, 3);

    cpu_reset();

    // Trigger NMI; next step should service NMI
    cpu_nmi();
    step_n(1);
    ASSERT_EQ_U8(0x11, cpu_get_a(), "NMI handler ran");

    // Now trigger IRQ (I flag should be set by interrupt enter; clear it)
    // CLI to enable IRQs
    tb_load_program(cpu_get_pc(), (const uint8_t[]){ 0x58 }, 1); // CLI
    step_n(1);
    cpu_irq();
    step_n(1);
    ASSERT_EQ_U8(0x22, cpu_get_a(), "IRQ handler ran");

    return 0;
}

int main(void) {
    int rc = 0;

    printf("CPU test: reset + basic load/store...\n");
    rc = test_reset_vector_and_basic_load_store();
    if (rc) return rc; else printf("  OK\n");

    printf("CPU test: page-cross penalty on read-indexed...\n");
    rc = test_page_cross_penalty_on_read_indexed();
    if (rc) return rc; else printf("  OK\n");

    printf("CPU test: branches taken & page-cross...\n");
    rc = test_branch_taken_and_page_cross();
    if (rc) return rc; else printf("  OK\n");

    printf("CPU test: JSR/RTS and stack...\n");
    rc = test_jsr_rts_stack();
    if (rc) return rc; else printf("  OK\n");

    printf("CPU test: NMI/IRQ vectors...\n");
    rc = test_nmi_irq_paths();
    if (rc) return rc; else printf("  OK\n");

    printf("All CPU tests passed.\n");
    return 0;
}
