#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cartridge.h"
#include "bus.h"
#include "cpu.h"


static inline uint16_t cpu_pc(void);
static inline uint8_t cpu_a(void);
static inline uint8_t cpu_x(void);
static inline uint8_t cpu_y(void);
static inline uint8_t cpu_p(void);
static inline uint8_t cpu_sp(void);

static inline uint16_t cpu_pc(void) { return cpu_get_pc(); }
static inline uint8_t cpu_a(void) { return cpu_get_a(); }
static inline uint8_t cpu_x(void) { return cpu_get_x(); }
static inline uint8_t cpu_y(void) { return cpu_get_y(); }
static inline uint8_t cpu_p(void) { return cpu_get_p(); }
static inline uint8_t cpu_sp(void) { return cpu_get_sp(); }

// --- Helpers -----------------------------------------------------------------
static void dump_range(uint16_t start, uint16_t end)
{
    for (uint16_t a = start; a <= end; a += 16)
    {
        printf("%04X: ", a);
        for (int i = 0; i < 16 && (a + i) <= end; i++)
        {
            printf("%02X ", cpu_read((uint16_t)(a + i)));
        }
        puts("");
    }
}

static void print_flags(uint8_t P)
{
    // NVU B D I Z C (U is unused=1 on 6502 clones)
    printf("N=%d V=%d U=%d B=%d D=%d I=%d Z=%d C=%d\n",
        !!(P & 0x80), !!(P & 0x40), !!(P & 0x20), !!(P & 0x10),
        !!(P & 0x08), !!(P & 0x04), !!(P & 0x02), !!(P & 0x01));
}

static void print_cpu(const char* tag)
{
    printf("%s PC=%04X A=%02X X=%02X Y=%02X P=%02X SP=%02X\n",
        tag, cpu_pc(), cpu_a(), cpu_x(), cpu_y(), cpu_p(), cpu_sp());
    print_flags(cpu_p());
}

// --- Runner ------------------------------------------------------------------
static int run_until_brk_or_limit(int max_steps)
{
    for (int step = 0; step < max_steps; step++)
    {
        uint16_t pc = cpu_pc();
        uint8_t op = cpu_read(pc);

        printf("STEP %4d: PC=%04X op=%02X\n", step, pc, op);

        if (op == 0x00)
        {
            // BRK: emulate vector fetch by your CPU; here we just log and stop.
            printf("BRK at %04X\n", pc);
            return 0;
        }
        // Execute one instruction (advance PC internally)
        cpu_step();
    }
    return 0;
}

// --- Entry -------------------------------------------------------------------
int main(int argc, char** argv)
{
    const char* rom_path = (argc >= 2) ? argv[1] : "C:/Users/Seth/CLion/C/Projects/nes-emulator/roms/controllertest2.nes";
    int max_steps = (argc >= 3) ? atoi(argv[2]) : 200;

    printf("Loading ROM: %s\n", rom_path);
    if (cartridge_load(rom_path) != 0)
    {
        fprintf(stderr, "Failed to load cartridge.\n");
        return 1;
    }

    cpu_reset();
    bus_reset();

    printf("After reset PC=%04X\n", cpu_pc());

    printf("Dump around reset $%04X...$%04X:\n", cpu_pc(), cpu_pc() + 0x10);
    dump_range(cpu_pc(), (cpu_pc() + 0x10));

    run_until_brk_or_limit(max_steps);
    print_cpu("Final CPU:");

    cartridge_unload();
    return 0;

}