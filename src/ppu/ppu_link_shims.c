// src/ppu/ppu_shims.c
// This file used to define ppu_ctrl_reg/ppu_mask_reg/ppu_oam_dma,
// which now live in src/ppu/ppu_regs.c. We keep this unit to satisfy
// CMake lists, but with no duplicate symbols.

#include <stdint.h>
#include "ppu_regs.h"
#include "bus.h"

// If you had code here previously, it has been intentionally removed
// to avoid multiple-definition linker errors. All public PPU entry
// points are implemented in ppu_regs.c now.
