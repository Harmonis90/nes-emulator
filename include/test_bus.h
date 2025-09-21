// tests/test_bus.h
// Minimal mock bus for CPU unit tests (provides cpu_read/cpu_write and helpers)

#ifndef TEST_BUS_H
#define TEST_BUS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    // Read/write used by the CPU core
    uint8_t  cpu_read(uint16_t addr);
    void     cpu_write(uint16_t addr, uint8_t v);

    // Helpers for tests
    void     tb_reset_memory(void);
    void     tb_poke(uint16_t addr, uint8_t v);
    uint8_t  tb_peek(uint16_t addr);
    void     tb_load_program(uint16_t addr, const uint8_t* bytes, size_t len);
    void     tb_set_reset_vector(uint16_t addr);
    void     tb_set_nmi_vector(uint16_t addr);
    void     tb_set_irq_vector(uint16_t addr);

#ifdef __cplusplus
}
#endif

#endif // TEST_BUS_H
