//
// Created by Seth on 9/6/2025.
//

#ifndef NES_INES_H
#define NES_INES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif
    // Parse an iNES buffer and initialize the appropriate mapper.
    // Returns 1 on success, 0 on failure.
int ines_load(const uint8_t* rom, size_t len);


// Convenience: read a .nes file into memory (caller must free()).
// Returns NULL on error and sets *out_size = 0.
uint8_t* ines_read_file(const char* path, size_t* out_size);
#ifdef __cplusplus
}
#endif


#endif //NES_INES_H
