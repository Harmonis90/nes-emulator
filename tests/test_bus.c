// tests/test_bus.c
// Minimal mock bus: a flat 64KiB RAM image. No mirroring needed for unit tests.

#include "test_bus.h"

static uint8_t s_mem[0x10000];

uint8_t cpu_read(uint16_t addr) {
    return s_mem[addr];
}

void cpu_write(uint16_t addr, uint8_t v) {
    s_mem[addr] = v;
}

void tb_reset_memory(void) {
    for (size_t i = 0; i < sizeof(s_mem); ++i) s_mem[i] = 0x00;
}

void tb_poke(uint16_t addr, uint8_t v) {
    s_mem[addr] = v;
}

uint8_t tb_peek(uint16_t addr) {
    return s_mem[addr];
}

void tb_load_program(uint16_t addr, const uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; ++i) s_mem[addr + i] = bytes[i];
}

static void write_vec(uint16_t vec, uint16_t addr) {
    s_mem[vec + 0] = (uint8_t)(addr & 0xFF);
    s_mem[vec + 1] = (uint8_t)(addr >> 8);
}

void tb_set_reset_vector(uint16_t addr) { write_vec(0xFFFC, addr); }
void tb_set_nmi_vector(uint16_t addr)   { write_vec(0xFFFA, addr); }
void tb_set_irq_vector(uint16_t addr)   { write_vec(0xFFFE, addr); }
