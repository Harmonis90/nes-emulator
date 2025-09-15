//
// Mapper front-end dispatcher
//
#include <stdint.h>
#include <stdio.h>
#include "mapper.h"

// Single active mapper
static const struct MapperOps* ops = NULL;

int mapper_init(int mapper_id,
                const uint8_t* prg, size_t prg_size,
                const uint8_t* chr, size_t chr_size)
{
    switch (mapper_id) {
    case 0: // NROM
        ops = mapper_nrom_init(prg, prg_size, chr, chr_size);
        return ops != NULL;
    default:
        fprintf(stderr, "mapper: unsupported id %d\n", mapper_id);
        ops = NULL;
        return 0;
    }
}

// ---- CPU (PRG) dispatch ----
uint8_t mapper_cpu_read(uint16_t addr)
{
    return (ops && ops->cpu_read) ? ops->cpu_read(addr) : 0xFF;
}

void mapper_cpu_write(uint16_t addr, uint8_t value)
{
    if (ops && ops->cpu_write) ops->cpu_write(addr, value);
}

// ---- PPU (CHR) dispatch ----
uint8_t mapper_chr_read(uint16_t addr)
{
    return (ops && ops->chr_read) ? ops->chr_read(addr) : 0x00;
}

void mapper_chr_write(uint16_t addr, uint8_t value)
{
    if (ops && ops->chr_write) ops->chr_write(addr, value);
}
