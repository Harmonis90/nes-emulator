// src/input/controller.c
#include <stdio.h>
#include <stdint.h>
#include "controller.h"

// --- Debug toggles ---------------------------------------------------------
#ifndef DEBUG_CONTROLLER
#define DEBUG_CONTROLLER 0
#endif

// --- Auto-start: press Start on several first latches so SMB leaves title ---
#ifndef CONTROLLER_AUTOSTART_LATCHES
#define CONTROLLER_AUTOSTART_LATCHES 8   // number of 1->0 latches to inject Start
#endif

static uint8_t latched[2]   = {0,0};  // frontend state (bit1=pressed)
static uint8_t shift_reg[2] = {0,0};  // what reads shift out
static uint8_t strobe       = 0;      // $4016 bit0
static int     autostart_left = CONTROLLER_AUTOSTART_LATCHES; // remaining injections

// (optional) small read counters to help debug which bit was read
static int read_count[2] = {0,0};

void controller_reset(void)
{
    latched[0] = latched[1] = 0;
    shift_reg[0] = shift_reg[1] = 0;
    strobe = 0;
    autostart_left = CONTROLLER_AUTOSTART_LATCHES;
    read_count[0] = read_count[1] = 0;
}

void controller_set_state(int port, uint8_t state)
{
    if ((unsigned)port >= 2) return;
    latched[port] = state;

    // While strobe=1, hardware keeps latching live state
    if (strobe & 1) {
        shift_reg[0] = latched[0];
        shift_reg[1] = latched[1];
    }

#if DEBUG_CONTROLLER
    // Uncomment for chattier logs:
    // fprintf(stderr, "PAD%d(latched)=%02X\n", port, state);
#endif
}

void controller_write(uint16_t addr, uint8_t data)
{
    if (addr != 0x4016) return;
    uint8_t prev = strobe;
    strobe = (uint8_t)(data & 1);

#if DEBUG_CONTROLLER
    fprintf(stderr, "$4016<=%02X  (strobe %u -> %u)\n", data, prev & 1, strobe & 1);
#endif

    if ((prev & 1) && !(strobe & 1)) {
        // 1 -> 0: latch both pads into shift registers
        shift_reg[0] = latched[0];
        shift_reg[1] = latched[1];
        read_count[0] = read_count[1] = 0;

        // --- AUTO-START INJECTION: set Start (bit3) for a few latches ---
        if (autostart_left > 0) {
            shift_reg[0] |= 0x08;     // Start
            latched[0]   |= 0x08;     // also reflect in "current" state
            autostart_left--;
        }

#if DEBUG_CONTROLLER
        fprintf(stderr, "Latch: P0=%02X P1=%02X  %s\n",
                shift_reg[0], shift_reg[1],
                (autostart_left >= 0) ? "(auto-start may be active)" : "");
#endif
    }

    // While strobe high, keep them live
    if (strobe & 1) {
        shift_reg[0] = latched[0];
        shift_reg[1] = latched[1];
        read_count[0] = read_count[1] = 0;
    }
}

uint8_t controller_read(uint16_t addr)
{
    int port = (addr == 0x4016) ? 0 : 1;
    uint8_t bit0  = (uint8_t)(shift_reg[port] & 1);
    uint8_t value = (uint8_t)(0x40 | bit0);  // bit6 high; only bit0 matters

    if ((strobe & 1) == 0) {
        // After 8 reads, real NES shifts in 1s
        shift_reg[port] = (uint8_t)((shift_reg[port] >> 1) | 0x80);
        if (read_count[port] < 10) read_count[port]++;
    }

#if DEBUG_CONTROLLER
    if (read_count[port] <= 8) {
        static const char* btn[8] = {"A","B","Sel","Start","Up","Down","Left","Right"};
        int idx = read_count[port] - 1;
        if (idx < 0) idx = 0;
        fprintf(stderr, "Read %s #%d: bit=%d  (btn=%s)\n",
                port==0 ? "$4016" : "$4017", read_count[port], bit0, btn[idx]);
    }
#endif
    return value;
}
