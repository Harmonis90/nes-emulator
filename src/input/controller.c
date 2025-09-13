//
// Created by Seth on 8/30/2025.
//

#include "controller.h"
#include <stdint.h>

// Internal state
static uint8_t strobe = 0;

static uint8_t pad_state[2] = {0, 0}; // currently latched buttons
static uint8_t shift_reg[2] = {0, 0}; // shift register


void controller_reset(void)
{
    strobe = 0;
    pad_state[0] = 0;
    pad_state[1] = 0;
    shift_reg[0] = 0;
    shift_reg[1] = 0;
}

void controller_set_state(int pad, uint8_t state)
{
    if (pad < 0 || pad > 1) return;
    pad_state[pad] = state;
}

void controller_write(uint16_t addr, uint8_t val)
{
    if (addr == 0x4016)
    {
        uint8_t prev = strobe;
        strobe = val & 1;
        if (strobe)
        {
            // While strobe = 1 keep (re)latching
            shift_reg[0] = pad_state[0];
            shift_reg[1] = pad_state[1];
        }
        else if (prev && !strobe)
        {
            // falling edge: latch once
            shift_reg[0] = pad_state[0];
            shift_reg[1] = pad_state[1];
        }
    }
}

uint8_t controller_read(uint16_t addr)
{
    int pad = (addr == 0x4017) ? 1 : 0;
    uint8_t ret;

    if (strobe)
    {
        // continuously return A button
        ret = (pad_state[pad] & 1);
    }
    else
    {
        ret = (shift_reg[pad] & 1);
        shift_reg[pad] >>= 1;
    }
    // Many NES open-bus behaviors set high bits; set bit6 for safety
    return (uint8_t)(ret | 0x40);

}