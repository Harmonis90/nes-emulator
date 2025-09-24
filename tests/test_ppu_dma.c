#include <assert.h>
#include <stdio.h>

#include "cpu.h"
#include "bus.h"
#include "ppu.h"


int test_ppu_oam_dma(void) {
    // Arrange: fill $0300-$03FF with a pattern
    for (int i = 0; i < 256; i++) {
        cpu_write((uint16_t)(0x0300 + i), (uint8_t)(i ^ 0xA5));
    }

    // Record cycle parity before DMA (to decide 513 vs 514)
    int odd = cpu_cycles_parity(); // 0 even, 1 odd

    // Act: trigger DMA from page $03
    uint64_t c0 = cpu_get_cycles();
    cpu_write(0x4014, 0x03); // bus will call ppu_oam_dma(0x03) and stall
    uint64_t c1 = cpu_get_cycles();

    // Assert: stall length
    int expected = 513 + odd;
    assert((int)(c1 - c0) == expected);

    // Assert: OAM contents
    for (int i = 0; i < 256; i++) {
        uint8_t want = (uint8_t)(i ^ 0xA5);
        uint8_t got  = ppu_oam_read_byte((uint8_t)i);
        if (got != want) {
            fprintf(stderr, "OAM[%d] expected %02X got %02X\n", i, want, got);
            return 1;
        }
    }
    return 0;
}
