//
// Created by Seth on 8/30/2025.
//

#include <stdint.h>
#include <stdio.h>
#include "mapper.h"

// single active mapper
static const struct MapperOps* ops = NULL;

int mapper_init(int id, const uint8_t* prg, size_t prg_size, const uint8_t* chr, size_t chr_size)
{
    switch (id)
    {
    case 0:
        ops = mapper_nrom_init(prg, prg_size, chr, chr_size);
        return ops != NULL;

    default:
        fprintf(stderr, "mapper: unsupported id %d\n", id);
        return 0;
    }


}

uint8_t mapper_cpu_read(uint16_t addr) { return ops ? ops -> cpu_read(addr) : 0xFF; }
void mapper_cpu_write(uint16_t addr, uint8_t v) { if (ops) ops -> cpu_write(addr, v); }