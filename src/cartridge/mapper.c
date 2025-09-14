#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "mapper.h"

// Current mapper ops
typedef struct
{
    uint8_t (*cpu_read)(uint16_t);
    void (*cpu_write)(uint16_t, uint8_t);
    uint8_t (*chr_read)(uint16_t);
    void (*chr_write)(uint16_t, uint8_t);
    void (*reset)(void);
}MapperOps;

extern const MapperOps* mapper_nrom_get_ops(void);

static const MapperOps* g_ops = NULL;

void mapper_reset(void)
{
    if (g_ops && g_ops -> reset) g_ops -> reset();
}

uint8_t mapper_cpu_read(uint16_t addr)
{
    return g_ops && g_ops -> cpu_read ? g_ops -> cpu_read(addr) : 0xFF;
}

void mapper_cpu_write(uint16_t addr, uint8_t data)
{
    if (g_ops && g_ops -> cpu_write) g_ops -> cpu_write(addr, data);
}

uint8_t mapper_chr_read(uint16_t addr)
{
    return g_ops && g_ops -> chr_read ? g_ops -> chr_read(addr) : 0x00;
}

void mapper_chr_write(uint16_t addr, uint8_t data)
{
    if (g_ops && g_ops -> chr_write) g_ops -> chr_write(addr, data);
}

// ---- NROM hook from iNES loader ---------------------------------------------
void mapper_nrom_init(uint8_t* prg, size_t prg_size,
                    uint8_t* chr, size_t chr_size,
                    int chr_is_ram)
{
    extern void nrom_set_blobs(uint8_t* prg, size_t prg_size,
                                uint8_t* chr, size_t chr_size,
                                int chr_is_ram);

        nrom_set_blobs(prg, prg_size, chr, chr_size, chr_is_ram);
        g_ops = mapper_nrom_get_ops();
        mapper_reset();

}