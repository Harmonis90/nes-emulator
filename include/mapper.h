#pragma once
#include <stdint.h>
#include <stddef.h>

// Simple mapper dispatch API used by CPU/PPU back-ends
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
void mapper_nrom_init(uint8_t* prg, size_t prg_size, uint8_t* chr, size_t chr_size, int chr_is_ram);