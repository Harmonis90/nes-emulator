//
// Created by Seth on 8/30/2025.
//

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "ines.h"
#include "mapper.h"
#include "ppu_mem.h"

#define INES_HEADER_SIZE 16
#define TRAINER_SIZE 512




int ines_load(const uint8_t* rom, size_t len)
{
    if (!rom || len < INES_HEADER_SIZE)
    {
        fprintf(stderr, "ines_load: rom too small or NULL\n");
        return -1;
    }
    if (!(rom[0] == 'N' && rom[1] == 'E' && rom[2] == 'S' && rom[3] == 0x1A))
    {
        fprintf(stderr, "ines_load: bad magic (not iNES)\n");
        return -2;
    }
    uint8_t prg_banks = rom[4];
    uint8_t chr_banks = rom[5];
    uint8_t flag6 = rom[6];
    uint8_t flag7 = rom[7];

    int is_nes2 = ((flag7 & 0x0C) == 0x08);
    if (is_nes2)
    {
        // try to load as iNES 1.0

    }
    // Mapper id: high nibble from flags7, low nibble from flags6
    int mapper_id = ((flag7 & 0xF0) | (flag6 >> 4));

    // Mirroring
    int four_screen = (flag6 & 0x08) ? 1 : 0;
    int mirror_bit = (flag6 & 0x01) ? 1 : 0;

    mirroring_t mirroring;

    if (four_screen)
    {
        mirroring = MIRROR_FOUR;
    }
    else
    {
        mirroring = mirror_bit ? MIRROR_VERTICAL : MIRROR_HORIZONTAL;
    }
    // trainer?
    int has_trainer =  (flag6 & 0x04) ? 1 : 0;

    size_t prg_size = (size_t)prg_banks * 16384u;
    size_t chr_size = (size_t)chr_banks * 8192u;

    // Layout: [16-byte header][optional 512-byte trainer][PRG][CHR]
    size_t off = INES_HEADER_SIZE;
    if (has_trainer)
    {
        if (len < off + TRAINER_SIZE)
        {
            fprintf(stderr, "ines_load: file truncated in trainer\n");
            return -3;
        }
        off += TRAINER_SIZE;
    }
    // Check space for PRG
    if (len < off + prg_size)
    {
        fprintf(stderr, "ines_load: file truncated in PRG (need %zu)\n", prg_size);
        return -4;
    }
    const uint8_t* prg_ptr = rom + off;
    off += prg_size;

    // Check space for CHR (if present)
    const uint8_t* chr_ptr = NULL;
    if (chr_size > 0)
    {
        if (len < off + chr_size)
        {
            fprintf(stderr, "ines_load: file truncated in CHR (needs %zu)\n", chr_size);
            return -5;
        }
        chr_ptr = rom + off;
        off += chr_size;
    }
    // Minimal mapper support: NROM (0)
    if (mapper_id != 0)
    {
        fprintf(stderr, "ines_load: mapper %d not implemented (only NROM=0 supported now)\n", mapper_id);
        return -6;
    }
    // Initialize mapper (NROM). If chr_banks==0 => CHR-RAM.
    int chr_is_ram = (chr_size == 0) ? 1 : 0;
    mapper_nrom_init((uint8_t*)prg_ptr, prg_size,
                    (uint8_t*)chr_ptr, chr_size,
                    chr_is_ram);

    // Apply nametable mirroring to PPU memory
    ppu_mem_set_mirroring(mirroring);

    const char* mirror_str =
        (mirroring == MIRROR_HORIZONTAL) ? "HORIZ" :
        (mirroring == MIRROR_VERTICAL) ? "VERT" :
        (mirroring == MIRROR_SINGLE_LO) ? "SINGLE_LO" :
        (mirroring == MIRROR_SINGLE_HI) ? "SINGLE_HI" :
        (mirroring == MIRROR_FOUR) ? "FOUR" : "UNKNOWN";

    printf("ines: mapper=%d, PRG=%zuKB, CHR=%zuKB, mirroring=%s, trainer=%s\n",
        mapper_id, prg_size / 1024, chr_size / 1024, mirror_str, has_trainer ? "yes" : "no");

    return 0;
}