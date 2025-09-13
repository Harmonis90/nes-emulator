//
// Created by Seth on 9/8/2025.
//
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    uint8_t rom[16 + 0x4000];
    memset(rom, 0, sizeof(rom));

    // iNES header
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4]=1; // 1×16KB PRG
    rom[5]=0; // 0×CHR

    uint8_t* prg = rom+16;
    int i=0;

    // Program starts at $8000:
    prg[i++] = 0x18;              // CLC          (C=0)
    prg[i++] = 0xB0; prg[i++] = 0x02; // BCS +2 (not taken)
    prg[i++] = 0x38;              // SEC          (C=1)
    prg[i++] = 0x90; prg[i++] = 0x02; // BCC +2 (not taken)

    prg[i++] = 0xA9; prg[i++] = 0x00; // LDA #$00   (Z=1, N=0)
    prg[i++] = 0xF0; prg[i++] = 0x02; // BEQ +2 (taken, skips next)
    prg[i++] = 0xA9; prg[i++] = 0xFF; // LDA #$FF   (should skip)
    prg[i++] = 0x30; prg[i++] = 0x02; // BMI +2 (not taken, N=0)

    prg[i++] = 0x10; prg[i++] = 0x02; // BPL +2 (taken, skips next)
    prg[i++] = 0xA9; prg[i++] = 0x55; // LDA #$55   (should skip)
    prg[i++] = 0xA9; prg[i++] = 0x7F; // LDA #$7F   (Z=0, N=0)

    prg[i++] = 0xA9; prg[i++] = 0x40; // LDA #$40   (positive)
    prg[i++] = 0x69; prg[i++] = 0x40; // ADC #$40   (A=0x80, N=1, V=1)
    prg[i++] = 0x70; prg[i++] = 0x02; // BVS +2 (taken, skips next)
    prg[i++] = 0xA9; prg[i++] = 0x11; // LDA #$11   (should skip)
    prg[i++] = 0x50; prg[i++] = 0x02; // BVC +2 (not taken, V=1)

    prg[i++] = 0x00;              // BRK

    // Vectors
    prg[0x3FFC]=0x00; prg[0x3FFD]=0x80; // RESET → $8000
    prg[0x3FFE]=0x00; prg[0x3FFF]=0x80; // IRQ/BRK → $8000

    FILE* f=fopen("roms/branchtest.nes","wb");
    fwrite(rom,1,sizeof(rom),f);
    fclose(f);
    return 0;
}
