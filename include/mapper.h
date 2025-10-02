#pragma once
#include <stdint.h>
#include <stddef.h>


struct MapperOps
{
    uint8_t (*cpu_read)(uint16_t addr);
    void (*cpu_write)(uint16_t addr, uint8_t val);
    uint8_t (*chr_read)(uint16_t addr);
    void (*chr_write)(uint16_t addr, uint8_t val);
};

// Initialize the active mapper with PRG/CHR blobs.
// Returns 1 on success, 0 on failure.
int mapper_init(int mapper_id, const uint8_t* prg, size_t prg_size, const uint8_t* chr, size_t chr_size);
// Simple mapper dispatch API used by CPU/PPU back-ends

// mapper 4 init
const struct MapperOps* mapper_mmc3_init(const uint8_t* prg, size_t prg_size,
                                         const uint8_t* chr, size_t chr_size);


void mapper_reset(void);

// CPU <-> PRG
uint8_t mapper_cpu_read(uint16_t addr);
void mapper_cpu_write(uint16_t addr, uint8_t data);

// PPU <-> CHR (pattern tables)
uint8_t mapper_chr_read(uint16_t addr);
void mapper_chr_write(uint16_t addr, uint8_t data);

// ---- NROM initializer --------------------------------------------------------
// prg       : pointer to PRG data (size = 16KB or 32KB)
// prg_size  : 16384 (NROM-128 mirror) or 32768 (NROM-256)
// chr       : pointer to CHR blob; may be NULL if CHR-RAM cart
// chr_size  : 0 (for 8KB CHR-RAM) or 8192 (for CHR-ROM)
// chr_is_ram: non-zero if CHR should be writable (CHR-RAM); otherwise ROM

// ---- Specific mapper(s) ----
// Mapper 0 (NROM) factory: returns the ops table, or NULL on failure.
const struct MapperOps* mapper_nrom_init(const uint8_t* prg_data, size_t prg_len,
                                         const uint8_t* chr_data, size_t chr_len);

