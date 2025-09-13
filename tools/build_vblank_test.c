//
// Created by Seth on 9/9/2025.
//

#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void) {
    uint8_t rom[16 + 0x4000];                 // iNES header + 16KB PRG
    memset(rom, 0, sizeof(rom));

    // iNES header (NROM-128)
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4]=1;   // PRG: 1 x 16KB
    rom[5]=0;   // CHR: 0
    // flags6/7 = 0

    uint8_t* prg = rom + 16;
    int i = 0;

    // ---- $8000 program ----
    prg[i++] = 0x78;                   // SEI
    prg[i++] = 0xD8;                   // CLD
    prg[i++] = 0xA9; prg[i++] = 0x00;  // LDA #$00
    prg[i++] = 0x8D; prg[i++] = 0x00; prg[i++] = 0x20; // STA $2000

    // wait_vblank:
    prg[i++] = 0xAD; prg[i++] = 0x02; prg[i++] = 0x20; // LDA $2002
    prg[i++] = 0x10; prg[i++] = 0xFB; // BPL -5 (loop while VBlank bit7=0)

    // success path:
    prg[i++] = 0xA9; prg[i++] = 0xAA; // LDA #$AA
    prg[i++] = 0x8D; prg[i++] = 0x00; prg[i++] = 0x60; // STA $6000
    prg[i++] = 0x00;                   // BRK

    // vectors (for 16KB PRG: map to $FFFA-$FFFF at PRG[$3FFA-$3FFF])
    prg[0x3FFA] = 0x00; prg[0x3FFB] = 0x80; // NMI  -> $8000 (optional)
    prg[0x3FFC] = 0x00; prg[0x3FFD] = 0x80; // RESET-> $8000
    prg[0x3FFE] = 0x00; prg[0x3FFF] = 0x80; // IRQ  -> $8000

    // write file under build dir's roms/
    FILE* f = fopen("roms/vblanktest.nes", "wb");
    if (!f) return 1;
    fwrite(rom, 1, sizeof(rom), f);
    fclose(f);
    return 0;
}

