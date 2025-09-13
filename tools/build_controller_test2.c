// tools/build_controller_test2.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    uint8_t rom[16 + 0x4000];
    memset(rom, 0, sizeof(rom));

    // iNES header (NROM-128)
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4]=1;   // 1x16KB PRG
    rom[5]=0;   // 0xCHR

    uint8_t* prg = rom + 16;
    int i = 0;

    // Program @ $8000
    prg[i++] = 0x78;                         // SEI

    // --- Phase 1: strobe=0, shift into $6000..$6007 ---
    prg[i++] = 0xA9; prg[i++] = 0x01;        // LDA #$01
    prg[i++] = 0x8D; prg[i++] = 0x16; prg[i++] = 0x40; // STA $4016 (strobe=1)
    prg[i++] = 0xA9; prg[i++] = 0x00;        // LDA #$00
    prg[i++] = 0x8D; prg[i++] = 0x16; prg[i++] = 0x40; // STA $4016 (strobe=0, latch)
    prg[i++] = 0xA2; prg[i++] = 0x00;        // LDX #$00
    // loop1:
    prg[i++] = 0xAD; prg[i++] = 0x16; prg[i++] = 0x40; // LDA $4016
    prg[i++] = 0x29; prg[i++] = 0x01;        // AND #$01
    prg[i++] = 0x9D; prg[i++] = 0x00; prg[i++] = 0x60; // STA $6000,X
    prg[i++] = 0xE8;                         // INX
    prg[i++] = 0xE0; prg[i++] = 0x08;        // CPX #$08
    prg[i++] = 0xD0; prg[i++] = 0xF3;        // BNE loop1 (-13 to LDA $4016)

    // --- Phase 2: strobe=1, continuous A -> $6010..$6017 ---
    prg[i++] = 0xA9; prg[i++] = 0x01;        // LDA #$01
    prg[i++] = 0x8D; prg[i++] = 0x16; prg[i++] = 0x40; // STA $4016 (strobe=1)
    prg[i++] = 0xA2; prg[i++] = 0x00;        // LDX #$00
    // loop2:
    prg[i++] = 0xAD; prg[i++] = 0x16; prg[i++] = 0x40; // LDA $4016
    prg[i++] = 0x29; prg[i++] = 0x01;        // AND #$01
    prg[i++] = 0x9D; prg[i++] = 0x10; prg[i++] = 0x60; // STA $6010,X
    prg[i++] = 0xE8;                         // INX
    prg[i++] = 0xE0; prg[i++] = 0x08;        // CPX #$08
    prg[i++] = 0xD0; prg[i++] = 0xF3;        // BNE loop2 (-13 to LDA $4016)

    // --- Phase 3: strobe=0 again, read pad2 ($4017) -> $6020..$6027 ---
    prg[i++] = 0xA9; prg[i++] = 0x00;        // LDA #$00
    prg[i++] = 0x8D; prg[i++] = 0x16; prg[i++] = 0x40; // STA $4016 (strobe=0, latch)
    prg[i++] = 0xA2; prg[i++] = 0x00;        // LDX #$00
    // loop3:
    prg[i++] = 0xAD; prg[i++] = 0x17; prg[i++] = 0x40; // LDA $4017
    prg[i++] = 0x29; prg[i++] = 0x01;        // AND #$01
    prg[i++] = 0x9D; prg[i++] = 0x20; prg[i++] = 0x60; // STA $6020,X
    prg[i++] = 0xE8;                         // INX
    prg[i++] = 0xE0; prg[i++] = 0x08;        // CPX #$08
    prg[i++] = 0xD0; prg[i++] = 0xF3;        // BNE loop3 (-13 to LDA $4017)

    // Done
    prg[i++] = 0x00;                         // BRK

    // Vectors
    prg[0x3FFA] = 0x00; prg[0x3FFB] = 0x80;  // NMI  -> $8000
    prg[0x3FFC] = 0x00; prg[0x3FFD] = 0x80;  // RESET-> $8000
    prg[0x3FFE] = 0x00; prg[0x3FFF] = 0x80;  // IRQ  -> $8000

    FILE* f = fopen("roms/controllertest2.nes", "wb");
    if (!f) return 1;
    fwrite(rom, 1, sizeof(rom), f);
    fclose(f);
    return 0;
}
