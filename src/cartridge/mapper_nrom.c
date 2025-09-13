//
// Created by Seth on 8/30/2025.
//

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mapper.h"
#include "bus.h"

static uint8_t prg[0x8000];
static size_t prg_size = 0;

static uint8_t nrom_read(uint16_t addr)
{
    if (addr >= 0x8000)
    {
        size_t idx = (prg_size == 0x4000) ? (addr - 0x8000) & 0x3FFF : (addr - 0x8000);
        return prg[idx];
    }
    return cpu_read(addr);
}

static void nrom_write(uint16_t addr, uint8_t v)
{
    if (addr >= 0x8000)
    {
        // ROM normally read-only, ignore
        return;
    }
    cpu_write(addr, v);
}

static struct MapperOps nrom_ops = {
    .cpu_read = nrom_read,
    .cpu_write = nrom_write

};

const struct MapperOps* mapper_nrom_init(const uint8_t* prg_data, size_t prg_len, const uint8_t* chr, size_t chr_len)
{
    if (prg_len != 0x4000 && prg_len != 0x8000) return NULL;

    memcpy(prg, prg_data, prg_len);

    if (prg_len == 0x4000) memcpy(prg + 0x4000, prg, 0x4000);

    prg_size =  prg_len;
    return &nrom_ops;
}