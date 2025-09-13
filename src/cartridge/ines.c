//
// Created by Seth on 8/30/2025.
//

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ines.h"
#include "mapper.h"

static int has_trainer(const uint8_t* d) { return (d[6] & 0x04) != 0; }
static int is_ines(const uint8_t* d, size_t n)
{
    return n >= 16 && d[0] == 'N' && d[1] == 'E' && d[2] == 'S' && d[3] == 0x1A;
}

int ines_load(const uint8_t* data, size_t len)
{
    if (!is_ines(data, len))
    {
        fprintf(stderr, "ines: bad header\n");
        return 0;
    }

    uint8_t prg16k = data[4];
    uint8_t chr8k = data[5];
    uint8_t flag6 = data[6];
    uint8_t flag7 = data[7];
    int mapper = ((flag7 & 0xF0) | (flag6 >> 4));

    size_t header = 16;
    size_t trainer = has_trainer(data) ? 512 : 0;
    size_t prg_size = (size_t)prg16k * 0x4000;
    size_t chr_size = (size_t)chr8k * 0x2000;

    if (len < header + trainer + prg_size + chr_size)
    {
        fprintf(stderr, "ines: truncated file\n");
        return 0;
    }

    const uint8_t* prg = data + header + trainer;
    const uint8_t* chr = prg + prg_size;

    fprintf(stderr, "ines: mapper=%d, PRG=%zuKB, CHR=%zuKB, mirroring=%s, trainer=%s\n",
        mapper, prg_size / 1024, chr_size / 1024,
        (flag6 & 0x01) ? "VERT" : "HORZ",
        has_trainer(data) ? "yes" : "no");

    return mapper_init(mapper, prg, prg_size, chr, chr_size);
}