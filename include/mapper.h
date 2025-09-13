//
// Created by Seth on 9/6/2025.
//

#ifndef NES_MAPPER_H
#define NES_MAPPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

    // ---- Mapper interface ----
struct MapperOps
{
    uint8_t (*cpu_read)(uint16_t addr);
    void (*cpu_write)(uint16_t addr, uint8_t value);
    // (PPU side can be added later: ppu_read/ppu_write for CHR)

};
    // Initialize the active mapper with PRG/CHR blobs.
    // Returns 1 on success, 0 on failure.
int mapper_init(int mapper_id,
                const uint8_t* prg, size_t prg_size,
                const uint8_t* chr, size_t chr_size);

    // Active mapper CPU bus entry points
uint8_t mapper_cpu_read(uint16_t addr);
void mapper_cpu_write(uint16_t addr, uint8_t value);

    // ---- Specific mapper(s) exposed to mapper.c ----
    // Mapper 0 (NROM) factory: returns the ops table, or NULL on failure.
const struct MapperOps* mapper_nrom_init(const uint8_t* prg_data, size_t prg_len,
                                        const uint8_t* chr_data, size_t chr_len);

#ifdef __cplusplus
}
#endif
#endif //NES_MAPPER_H
