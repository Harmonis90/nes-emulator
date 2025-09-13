#pragma once
#include <stdint.h>
#include "ppu_mem.h"

int cartridge_load(const char *path);
void cartridge_unload(void);
