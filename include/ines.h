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
int ines_load(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif


#endif //NES_INES_H
