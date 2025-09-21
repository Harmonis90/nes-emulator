#include <stdint.h>

#include "cpu.h"




void cpu_dma_stall(int cycles)
{
    if (cycles > 0)
    {
        cpu_cycles_add(cycles);
    }
}

int cpu_cycles_parity(void)
{
    return (int)(cpu_get_cycles() & 1u);
}