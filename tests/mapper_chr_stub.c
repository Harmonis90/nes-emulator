// A tiny 8 KB CHR-RAM so ppu_mem can store/fetch pattern data in this test.
#include <stdint.h>

static uint8_t g_chr[0x2000]; // 8 KB

uint8_t mapper_chr_read(uint16_t addr) {
    return g_chr[addr & 0x1FFF];
}
void mapper_chr_write(uint16_t addr, uint8_t v) {
    g_chr[addr & 0x1FFF] = v;
}
