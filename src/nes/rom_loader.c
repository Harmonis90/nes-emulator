#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "ines.h"
#include "nes.h"


static uint8_t* read_file_all(const char* path, size_t* out_size)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t* buf = (uint8_t*)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_size = (size_t)n;
    return buf;
}

int nes_load_rom_file(const char* path)
{
    size_t size = 0;
    uint8_t* data = read_file_all(path, &size);
    if (!data) return 0;
    int ok = ines_load(data, size);
    free(data);
    return ok ? 1 : 0;
}