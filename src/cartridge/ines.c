#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include "ines.h"
#include "mapper.h"
#include "ppu_mem.h"

#define INES_HEADER_SIZE 16
#define TRAINER_SIZE 512

static int is_ines1(const uint8_t* rom, size_t len) {
    return rom && len >= INES_HEADER_SIZE &&
           rom[0] == 'N' && rom[1] == 'E' && rom[2] == 'S' && rom[3] == 0x1A;
}

uint8_t* ines_read_file(const char* path, size_t* out_size)
{
    if (out_size) *out_size = 0;
    if (!path) return NULL;

    FILE* f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }

    long n = ftell(f);
    if (n <= 0) {
        fprintf(stderr, "ines_read_file: empty or unreadable file\n");
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }

    uint8_t* buf = (uint8_t*)malloc((size_t)n);
    if (!buf) {
        fprintf(stderr, "ines_read_file: OOM allocating %ld bytes\n", n);
        fclose(f);
        return NULL;
    }

    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);

    if (rd != (size_t)n) {
        fprintf(stderr, "ines_read_file: short read (%zu/%ld)\n", rd, n);
        free(buf);
        return NULL;
    }

    if (out_size) *out_size = (size_t)n;
    return buf;
}


int ines_load(const uint8_t* rom, size_t len)
{
    if (!is_ines1(rom, len)) {
        fprintf(stderr, "ines_load: bad header\n");
        return 0; // failure
    }

    const uint8_t prg_banks = rom[4];
    const uint8_t chr_banks = rom[5];
    const uint8_t flag6     = rom[6];
    const uint8_t flag7     = rom[7];

    // Mapper id: high nibble from flag7, low nibble from flag6
    const int mapper_id = ((flag7 & 0xF0) | (flag6 >> 4));

    // Mirroring selection
    const int four_screen = (flag6 & 0x08) != 0;
    const int mirror_bit  = (flag6 & 0x01) != 0;

    mirroring_t mirroring = four_screen
        ? MIRROR_FOUR
        : (mirror_bit ? MIRROR_VERTICAL : MIRROR_HORIZONTAL);

    // Sizes
    const size_t prg_size = (size_t)prg_banks * 16384u; // 16KB units
    const size_t chr_size = (size_t)chr_banks * 8192u;  // 8KB units

    // Layout: [16-byte header][optional 512-byte trainer][PRG][CHR]
    size_t off = INES_HEADER_SIZE;
    const int has_trn = (flag6 & 0x04) != 0;
    if (has_trn) {
        if (len < off + TRAINER_SIZE) {
            fprintf(stderr, "ines_load: truncated in trainer\n");
            return 0;
        }
        off += TRAINER_SIZE;
    }

    // PRG
    if (len < off + prg_size) {
        fprintf(stderr, "ines_load: truncated in PRG (need %zu)\n", prg_size);
        return 0;
    }
    const uint8_t* prg_ptr = rom + off;
    off += prg_size;

    // CHR (may be zero for CHR-RAM carts)
    const uint8_t* chr_ptr = NULL;
    if (chr_size) {
        if (len < off + chr_size) {
            fprintf(stderr, "ines_load: truncated in CHR (need %zu)\n", chr_size);
            return 0;
        }
        chr_ptr = rom + off;
        off += chr_size;
    }

    // Initialize active mapper (handles NROM=0 and future mappers)
    if (!mapper_init(mapper_id, prg_ptr, prg_size, chr_ptr, chr_size)) {
        fprintf(stderr, "ines_load: mapper %d not supported/init failed\n", mapper_id);
        return 0;
    }

    // Apply nametable mirroring to PPU memory
    ppu_mem_set_mirroring(mirroring);

    const char* mirror_str =
        (mirroring == MIRROR_HORIZONTAL) ? "HORIZ" :
        (mirroring == MIRROR_VERTICAL)   ? "VERT"  :
        (mirroring == MIRROR_SINGLE_LO)  ? "SINGLE_LO" :
        (mirroring == MIRROR_SINGLE_HI)  ? "SINGLE_HI" :
        (mirroring == MIRROR_FOUR)       ? "FOUR"  : "UNKNOWN";

    printf("ines: mapper=%d, PRG=%zuKB, CHR=%zuKB, mirroring=%s, trainer=%s\n",
           mapper_id, prg_size / 1024, chr_size / 1024,
           mirror_str, has_trn ? "yes" : "no");

    return 1; // success
}
