//
// Created by Seth on 8/30/2025.
//

#ifndef NES_CONTROLLER_H
#define NES_CONTROLLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

void controller_reset(void);
uint8_t controller_read(uint16_t addr);
void controller_write(uint16_t addr, uint8_t val);
void controller_set_state(int pad, uint8_t state);


#ifdef __cplusplus
}
#endif
#endif //NES_CONTROLLER_H
