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
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "test_bus.h"

#ifndef ASSERT_EQ
#include <stdio.h>
#include <stdlib.h>
#define ASSERT_EQ(label, got, expect)                                      \
do {                                                                    \
if ((got) != (expect)) {                                            \
fprintf(stderr, "ASSERT FAILED: %s expected %04X got %04X\n",   \
(label), (unsigned)(expect), (unsigned)(got));          \
abort();                                                        \
}                                                                   \
} while (0)
#endif





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


void test_branch_taken_and_page_cross(void)
{
    // 1) Fresh memory/CPU
    tb_reset_memory();
    cpu_reset();

    // 2) Seed the first tiny program at $A000:
    //    A000: A9 00    LDA #$00  -> Z=1
    //    A002: F0 7D    BEQ +$7D  -> target = A004 + 0x7D = A081
    //    A004: 00       BRK       (should be skipped by the branch)
    const uint8_t prog[] = {
        0xA9, 0x00,       // LDA #$00
        0xF0, 0x7D,       // BEQ +$7D  (A004 + 0x7D = A081)
        0x00              // BRK       (skip)
    };
    tb_load_program(0xA000, prog, sizeof(prog));

    // 3) *** Place the post-branch block EXACTLY at the branch target $A081 ***
    //    A081: A9 01    LDA #$01
    //    A083: 10 7D    BPL +$7D
    //    A085: 00       BRK
    tb_poke(0xA081, 0xA9); // LDA #$imm
    tb_poke(0xA082, 0x01); // imm = 1
    tb_poke(0xA083, 0x10); // BPL
    tb_poke(0xA084, 0x7D); // +$7D
    tb_poke(0xA085, 0x00); // BRK

    // (Optional but common in these tests) Fill padding up to the second target and put a BRK there.
    for (uint16_t a = 0xA086; a < 0xA102; ++a) tb_poke(a, 0xEA); // NOPs
    tb_poke(0xA102, 0x00); // BRK at the second branch target

    // Start execution at A000 (if your reset doesn’t already do so)
    cpu_set_pc(0xA000);

    // 4) Execute:
    //    - Step LDA #$00   (A==0, Z=1)
    //    - Step BEQ +$7D   (taken -> PC=A081)
    step_n(2);

    // Now the next instruction at PC=A081 should be LDA #$01.
    // Step once to execute it and then assert A == 1.
    step_n(1);

    // 5) Assert post-branch LDA result
    uint8_t a = cpu_get_a();
    if (a != 1) {
        fprintf(stderr, "ASSERT FAILED: LDA post-branch expected 1 got %u (line %d)\n",
                (unsigned)a, __LINE__);
        abort();
    }

    // (Optional) You can keep stepping to cover the BPL and final BRK if your test wants to.
    // step_n(2); // BPL (likely taken) then BRK at A102
}


static void write_jsr_program(void)
{
    // Call site at B000:
    //   B000: 20 04 B0   JSR $B004
    //   B003: 00         BRK (return lands here)
    tb_poke(0xB000, 0x20);   // JSR
    tb_poke(0xB001, 0x04);   // low($B004)
    tb_poke(0xB002, 0xB0);   // high($B004)
    tb_poke(0xB003, 0x00);   // BRK (should run after RTS)

    // Subroutine at B004:
    //   B004: A9 42      LDA #$42
    //   B006: 60         RTS
    tb_poke(0xB004, 0xA9);   // LDA #imm
    tb_poke(0xB005, 0x42);   // imm = 0x42
    tb_poke(0xB006, 0x60);   // RTS
}

int test_jsr_rts_stack(void)
{
    tb_reset_memory();
    cpu_reset();

    write_jsr_program();
    cpu_set_pc(0xB000);

    // SP on reset is typically 0xFD (but don't hardcode; read it)
    uint8_t sp0 = cpu_get_sp();

    // Step JSR
    step_n(1);

    // After JSR:
    // - PC should be B004
    // - SP should be sp0 - 2 (two bytes pushed: hi then lo of return addr)
    // - Return address pushed should be B002 (address of last byte of JSR)
    ASSERT_EQ("PC after JSR", cpu_get_pc(), 0xB004);
    uint8_t sp1 = cpu_get_sp();
    ASSERT_EQ("SP after JSR", sp1, (uint8_t)(sp0 - 2));

    // Stack layout after JSR (6502 pushes hi then lo, stack grows downward):
    //   [0x0100 + sp0]     = hi(return = B002 -> 0xB0)
    //   [0x0100 + sp0 - 1] = lo(return = B002 -> 0x02)
    // After the two pushes, SP = sp0 - 2
    uint8_t ret_hi = cpu_read((uint16_t)(0x0100u + sp1 + 2)); // = 0x0100 + sp0
    uint8_t ret_lo = cpu_read((uint16_t)(0x0100u + sp1 + 1)); // = 0x0100 + sp0 - 1
    ASSERT_EQ("Return HI on stack", ret_hi, 0xB0);
    ASSERT_EQ("Return LO on stack", ret_lo, 0x02);

    // Step LDA #$42 inside subroutine
    step_n(1);
    ASSERT_EQ("A after LDA #$42", cpu_get_a(), 0x42);

    // Step RTS
    step_n(1);

    // After RTS:
    // - PC should be B003 (ret+1 from B002)
    // - SP restored to original
    ASSERT_EQ("PC after RTS", cpu_get_pc(), 0xB003);
    ASSERT_EQ("SP restored after RTS", cpu_get_sp(), sp0);

    // Optional: execute BRK to finish
    step_n(1);

    return 0; // success
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
    test_branch_taken_and_page_cross();
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
