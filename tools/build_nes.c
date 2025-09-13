//
// Created by Seth on 9/7/2025.
//

#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void) {
    uint8_t rom[16 + 0x4000]; // header + 16KB PRG

    // iNES header
    memset(rom, 0, sizeof(rom));
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4]=1; // 1x16KB PRG
    rom[5]=0; // 0xCHR

    uint8_t* prg = rom+16;
    // Program: LDA #$01; STA $6000; BRK
    prg[0]=0xA9; prg[1]=0x01;
    prg[2]=0x8D; prg[3]=0x00; prg[4]=0x60;
    prg[5]=0x00;

    // vectors
    prg[0x3FFC]=0x00; prg[0x3FFD]=0x80; // RESET -> $8000
    prg[0x3FFE]=0x00; prg[0x3FFF]=0x80; // IRQ/BRK -> $8000

    FILE* f=fopen("roms/minitest.nes","wb");
    fwrite(rom,1,sizeof(rom),f);
    fclose(f);

    return 0;
}
