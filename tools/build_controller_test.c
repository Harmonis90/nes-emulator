//
// Created by Seth on 9/9/2025.
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    uint8_t rom[16 + 0x4000];                // iNES header + 16KB PRG
    memset(rom, 0, sizeof(rom));

    // ---- iNES header (NROM-128) ----
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4]=1;   // 1 × 16KB PRG
    rom[5]=0;   // 0 × 8KB CHR
    // flags6/7 = 0

    uint8_t* prg = rom + 16;
    int i = 0;

    // ---- $8000 program ----
    // Latch controller, read 8 bits from $4016, store to $6000..$6007, then BRK.
    prg[i++] = 0x78;                   // SEI
    prg[i++] = 0xA9; prg[i++] = 0x01;  // LDA #$01
    prg[i++] = 0x8D; prg[i++] = 0x16; prg[i++] = 0x40; // STA $4016 (strobe=1)
    prg[i++] = 0xA9; prg[i++] = 0x00;  // LDA #$00
    prg[i++] = 0x8D; prg[i++] = 0x16; prg[i++] = 0x40; // STA $4016 (strobe=0, latch)

    prg[i++] = 0xA2; prg[i++] = 0x00;  // LDX #$00
    // loop:
    prg[i++] = 0xAD; prg[i++] = 0x16; prg[i++] = 0x40; // LDA $4016
    prg[i++] = 0x29; prg[i++] = 0x01;  // AND #$01  (mask out bit0)
    prg[i++] = 0x9D; prg[i++] = 0x00; prg[i++] = 0x60; // STA $6000,X
    prg[i++] = 0xE8;                   // INX
    prg[i++] = 0xE0; prg[i++] = 0x08;  // CPX #$08
    prg[i++] = 0xD0; prg[i++] = 0xF1;  // BNE loop (-15)

    prg[i++] = 0x00;                   // BRK

    // Vectors (for 16KB PRG, vectors live at PRG[$3FFA..$3FFF] → CPU $FFFA..$FFFF)
    prg[0x3FFA] = 0x00; prg[0x3FFB] = 0x80; // NMI  -> $8000
    prg[0x3FFC] = 0x00; prg[0x3FFD] = 0x80; // RESET-> $8000
    prg[0x3FFE] = 0x00; prg[0x3FFF] = 0x80; // IRQ  -> $8000

    FILE* f = fopen("roms/controllertest.nes", "wb");
    if (!f) return 1;
    fwrite(rom, 1, sizeof(rom), f);
    fclose(f);
    return 0;
}
