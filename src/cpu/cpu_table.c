// src/cpu/cpu_table.c
// Complete 6502 opcode tables: dispatch, base cycles, lengths, mnemonics, addrmodes.
// Illegal/undocumented opcodes are routed to op_illegal().

#include <stdint.h>
#include "cpu_table.h"

// -----------------------------------------------------------------------------
// Extern opcode handlers (provide these in cpu_ops.c)
// Names follow the pattern: <MNEM>_<mode> or simple mnem for implied.
// -----------------------------------------------------------------------------

// System / flow
extern void BRK(void); extern void RTI(void); extern void RTS(void);
extern void JSR_abs(void); extern void JMP_abs(void); extern void JMP_ind(void);

// Branches
extern void BPL(void); extern void BMI(void); extern void BVC(void); extern void BVS(void);
extern void BCC(void); extern void BCS(void); extern void BNE(void); extern void BEQ(void);

// Loads
extern void LDA_imm(void); extern void LDA_zp(void);  extern void LDA_zpx(void);
extern void LDA_abs(void); extern void LDA_abx(void); extern void LDA_aby(void);
extern void LDA_inx(void); extern void LDA_iny(void);

extern void LDX_imm(void); extern void LDX_zp(void);  extern void LDX_zpy(void);
extern void LDX_abs(void); extern void LDX_aby(void);

extern void LDY_imm(void); extern void LDY_zp(void);  extern void LDY_zpx(void);
extern void LDY_abs(void); extern void LDY_abx(void);

// Stores
extern void STA_zp(void);  extern void STA_zpx(void); extern void STA_abs(void);
extern void STA_abx(void); extern void STA_aby(void); extern void STA_inx(void); extern void STA_iny(void);

extern void STX_zp(void);  extern void STX_zpy(void); extern void STX_abs(void);
extern void STY_zp(void);  extern void STY_zpx(void); extern void STY_abs(void);

// ALU
extern void ADC_imm(void); extern void ADC_zp(void);  extern void ADC_zpx(void);
extern void ADC_abs(void); extern void ADC_abx(void); extern void ADC_aby(void);
extern void ADC_inx(void); extern void ADC_iny(void);

extern void SBC_imm(void); extern void SBC_zp(void);  extern void SBC_zpx(void);
extern void SBC_abs(void); extern void SBC_abx(void); extern void SBC_aby(void);
extern void SBC_inx(void); extern void SBC_iny(void);

extern void AND_imm(void); extern void AND_zp(void);  extern void AND_zpx(void);
extern void AND_abs(void); extern void AND_abx(void); extern void AND_aby(void);
extern void AND_inx(void); extern void AND_iny(void);

extern void ORA_imm(void); extern void ORA_zp(void);  extern void ORA_zpx(void);
extern void ORA_abs(void); extern void ORA_abx(void); extern void ORA_aby(void);
extern void ORA_inx(void); extern void ORA_iny(void);

extern void EOR_imm(void); extern void EOR_zp(void);  extern void EOR_zpx(void);
extern void EOR_abs(void); extern void EOR_abx(void); extern void EOR_aby(void);
extern void EOR_inx(void); extern void EOR_iny(void);

// Compares
extern void CMP_imm(void); extern void CMP_zp(void);  extern void CMP_zpx(void);
extern void CMP_abs(void); extern void CMP_abx(void); extern void CMP_aby(void);
extern void CMP_inx(void); extern void CMP_iny(void);

extern void CPX_imm(void); extern void CPX_zp(void);  extern void CPX_abs(void);
extern void CPY_imm(void); extern void CPY_zp(void);  extern void CPY_abs(void);

// Bit test
extern void BIT_zp(void);  extern void BIT_abs(void);

// Shifts / Rotates (A and memory)
extern void ASL_A(void);   extern void ASL_zp(void);  extern void ASL_zpx(void);
extern void ASL_abs(void); extern void ASL_abx(void);

extern void LSR_A(void);   extern void LSR_zp(void);  extern void LSR_zpx(void);
extern void LSR_abs(void); extern void LSR_abx(void);

extern void ROL_A(void);   extern void ROL_zp(void);  extern void ROL_zpx(void);
extern void ROL_abs(void); extern void ROL_abx(void);

extern void ROR_A(void);   extern void ROR_zp(void);  extern void ROR_zpx(void);
extern void ROR_abs(void); extern void ROR_abx(void);

// Inc/Dec
extern void INC_zp(void);  extern void INC_zpx(void); extern void INC_abs(void); extern void INC_abx(void);
extern void DEC_zp(void);  extern void DEC_zpx(void); extern void DEC_abs(void); extern void DEC_abx(void);
extern void INX(void);     extern void DEX(void);     extern void INY(void);     extern void DEY(void);

// Stack / transfers
extern void PHA(void); extern void PHP(void); extern void PLA(void); extern void PLP(void);
extern void TAX(void); extern void TXA(void); extern void TAY(void); extern void TYA(void);
extern void TSX(void); extern void TXS(void);

// Flags / NOP
extern void CLC(void); extern void SEC(void); extern void CLI(void); extern void SEI(void);
extern void CLD(void); extern void SED(void); extern void CLV(void);
extern void NOP(void);

// Illegal / unimplemented
extern void op_illegal(void);

// -----------------------------------------------------------------------------
// Helper macros
// -----------------------------------------------------------------------------
#define IL op_illegal

// -----------------------------------------------------------------------------
// Dispatch table (256)
// -----------------------------------------------------------------------------
const cpu_op_t cpu_dispatch[256] = {
/* 00 */ BRK,      /* 01 */ ORA_inx,  /* 02 */ IL,       /* 03 */ IL,
 /* 04 */ IL,       /* 05 */ ORA_zp,   /* 06 */ ASL_zp,  /* 07 */ IL,
 /* 08 */ PHP,      /* 09 */ ORA_imm,  /* 0A */ ASL_A,   /* 0B */ IL,
 /* 0C */ IL,       /* 0D */ ORA_abs,  /* 0E */ ASL_abs, /* 0F */ IL,

/* 10 */ BPL,      /* 11 */ ORA_iny,  /* 12 */ IL,       /* 13 */ IL,
 /* 14 */ IL,       /* 15 */ ORA_zpx,  /* 16 */ ASL_zpx, /* 17 */ IL,
 /* 18 */ CLC,      /* 19 */ ORA_aby,  /* 1A */ IL,       /* 1B */ IL,
 /* 1C */ IL,       /* 1D */ ORA_abx,  /* 1E */ ASL_abx, /* 1F */ IL,

/* 20 */ JSR_abs,  /* 21 */ AND_inx,  /* 22 */ IL,       /* 23 */ IL,
 /* 24 */ BIT_zp,   /* 25 */ AND_zp,   /* 26 */ ROL_zp,  /* 27 */ IL,
 /* 28 */ PLP,      /* 29 */ AND_imm,  /* 2A */ ROL_A,   /* 2B */ IL,
 /* 2C */ BIT_abs,  /* 2D */ AND_abs,  /* 2E */ ROL_abs, /* 2F */ IL,

/* 30 */ BMI,      /* 31 */ AND_iny,  /* 32 */ IL,       /* 33 */ IL,
 /* 34 */ IL,       /* 35 */ AND_zpx,  /* 36 */ ROL_zpx, /* 37 */ IL,
 /* 38 */ SEC,      /* 39 */ AND_aby,  /* 3A */ IL,       /* 3B */ IL,
 /* 3C */ IL,       /* 3D */ AND_abx,  /* 3E */ ROL_abx, /* 3F */ IL,

/* 40 */ RTI,      /* 41 */ EOR_inx,  /* 42 */ IL,       /* 43 */ IL,
 /* 44 */ IL,       /* 45 */ EOR_zp,   /* 46 */ LSR_zp,  /* 47 */ IL,
 /* 48 */ PHA,      /* 49 */ EOR_imm,  /* 4A */ LSR_A,   /* 4B */ IL,
 /* 4C */ JMP_abs,  /* 4D */ EOR_abs,  /* 4E */ LSR_abs, /* 4F */ IL,

/* 50 */ BVC,      /* 51 */ EOR_iny,  /* 52 */ IL,       /* 53 */ IL,
 /* 54 */ IL,       /* 55 */ EOR_zpx,  /* 56 */ LSR_zpx, /* 57 */ IL,
 /* 58 */ CLI,      /* 59 */ EOR_aby,  /* 5A */ IL,       /* 5B */ IL,
 /* 5C */ IL,       /* 5D */ EOR_abx,  /* 5E */ LSR_abx, /* 5F */ IL,

/* 60 */ RTS,      /* 61 */ ADC_inx,  /* 62 */ IL,       /* 63 */ IL,
 /* 64 */ IL,       /* 65 */ ADC_zp,   /* 66 */ ROR_zp,  /* 67 */ IL,
 /* 68 */ PLA,      /* 69 */ ADC_imm,  /* 6A */ ROR_A,   /* 6B */ IL,
 /* 6C */ JMP_ind,  /* 6D */ ADC_abs,  /* 6E */ ROR_abs, /* 6F */ IL,

/* 70 */ BVS,      /* 71 */ ADC_iny,  /* 72 */ IL,       /* 73 */ IL,
 /* 74 */ IL,       /* 75 */ ADC_zpx,  /* 76 */ ROR_zpx, /* 77 */ IL,
 /* 78 */ SEI,      /* 79 */ ADC_aby,  /* 7A */ IL,       /* 7B */ IL,
 /* 7C */ IL,       /* 7D */ ADC_abx,  /* 7E */ ROR_abx, /* 7F */ IL,

/* 80 */ IL,       /* 81 */ STA_inx,  /* 82 */ IL,       /* 83 */ IL,
 /* 84 */ STY_zp,   /* 85 */ STA_zp,   /* 86 */ STX_zp,  /* 87 */ IL,
 /* 88 */ DEY,      /* 89 */ IL,       /* 8A */ TXA,     /* 8B */ IL,
 /* 8C */ STY_abs,  /* 8D */ STA_abs,  /* 8E */ STX_abs, /* 8F */ IL,

/* 90 */ BCC,      /* 91 */ STA_iny,  /* 92 */ IL,       /* 93 */ IL,
 /* 94 */ STY_zpx,  /* 95 */ STA_zpx,  /* 96 */ STX_zpy, /* 97 */ IL,
 /* 98 */ TYA,      /* 99 */ STA_aby,  /* 9A */ TXS,     /* 9B */ IL,
 /* 9C */ IL,       /* 9D */ STA_abx,  /* 9E */ IL,       /* 9F */ IL,

/* A0 */ LDY_imm,  /* A1 */ LDA_inx,  /* A2 */ LDX_imm,  /* A3 */ IL,
 /* A4 */ LDY_zp,   /* A5 */ LDA_zp,   /* A6 */ LDX_zp,  /* A7 */ IL,
 /* A8 */ TAY,      /* A9 */ LDA_imm,  /* AA */ TAX,     /* AB */ IL,
 /* AC */ LDY_abs,  /* AD */ LDA_abs,  /* AE */ LDX_abs, /* AF */ IL,

/* B0 */ BCS,      /* B1 */ LDA_iny,  /* B2 */ IL,       /* B3 */ IL,
 /* B4 */ LDY_zpx,  /* B5 */ LDA_zpx,  /* B6 */ LDX_zpy, /* B7 */ IL,
 /* B8 */ CLV,      /* B9 */ LDA_aby,  /* BA */ TSX,     /* BB */ IL,
 /* BC */ LDY_abx,  /* BD */ LDA_abx,  /* BE */ LDX_aby, /* BF */ IL,

/* C0 */ CPY_imm,  /* C1 */ CMP_inx,  /* C2 */ IL,       /* C3 */ IL,
 /* C4 */ CPY_zp,   /* C5 */ CMP_zp,   /* C6 */ DEC_zp,  /* C7 */ IL,
 /* C8 */ INY,      /* C9 */ CMP_imm,  /* CA */ DEX,     /* CB */ IL,
 /* CC */ CPY_abs,  /* CD */ CMP_abs,  /* CE */ DEC_abs, /* CF */ IL,

/* D0 */ BNE,      /* D1 */ CMP_iny,  /* D2 */ IL,       /* D3 */ IL,
 /* D4 */ IL,       /* D5 */ CMP_zpx,  /* D6 */ DEC_zpx, /* D7 */ IL,
 /* D8 */ CLD,      /* D9 */ CMP_aby,  /* DA */ IL,       /* DB */ IL,
 /* DC */ IL,       /* DD */ CMP_abx,  /* DE */ DEC_abx, /* DF */ IL,

/* E0 */ CPX_imm,  /* E1 */ SBC_inx,  /* E2 */ IL,       /* E3 */ IL,
 /* E4 */ CPX_zp,   /* E5 */ SBC_zp,   /* E6 */ INC_zp,  /* E7 */ IL,
 /* E8 */ INX,      /* E9 */ SBC_imm,  /* EA */ NOP,     /* EB */ IL,
 /* EC */ CPX_abs,  /* ED */ SBC_abs,  /* EE */ INC_abs, /* EF */ IL,

/* F0 */ BEQ,      /* F1 */ SBC_iny,  /* F2 */ IL,       /* F3 */ IL,
 /* F4 */ IL,       /* F5 */ SBC_zpx,  /* F6 */ INC_zpx, /* F7 */ IL,
 /* F8 */ SED,      /* F9 */ SBC_aby,  /* FA */ IL,       /* FB */ IL,
 /* FC */ IL,       /* FD */ SBC_abx,  /* FE */ INC_abx, /* FF */ IL,
};

// -----------------------------------------------------------------------------
// Base cycles per opcode (no conditional +1 for page-cross/branches)
// -----------------------------------------------------------------------------
const uint8_t cpu_base_cycles[256] = {
/*00*/7, /*01*/6, /*02*/2, /*03*/2,  /*04*/2, /*05*/3, /*06*/5, /*07*/2,
 /*08*/3, /*09*/2, /*0A*/2, /*0B*/2,  /*0C*/2, /*0D*/4, /*0E*/6, /*0F*/2,

/*10*/2, /*11*/5, /*12*/2, /*13*/2,  /*14*/2, /*15*/4, /*16*/6, /*17*/2,
 /*18*/2, /*19*/4, /*1A*/2, /*1B*/2,  /*1C*/2, /*1D*/4, /*1E*/7, /*1F*/2,

/*20*/6, /*21*/6, /*22*/2, /*23*/2,  /*24*/3, /*25*/3, /*26*/5, /*27*/2,
 /*28*/4, /*29*/2, /*2A*/2, /*2B*/2,  /*2C*/4, /*2D*/4, /*2E*/6, /*2F*/2,

/*30*/2, /*31*/5, /*32*/2, /*33*/2,  /*34*/2, /*35*/4, /*36*/6, /*37*/2,
 /*38*/2, /*39*/4, /*3A*/2, /*3B*/2,  /*3C*/2, /*3D*/4, /*3E*/7, /*3F*/2,

/*40*/6, /*41*/6, /*42*/2, /*43*/2,  /*44*/2, /*45*/3, /*46*/5, /*47*/2,
 /*48*/3, /*49*/2, /*4A*/2, /*4B*/2,  /*4C*/3, /*4D*/4, /*4E*/6, /*4F*/2,

/*50*/2, /*51*/5, /*52*/2, /*53*/2,  /*54*/2, /*55*/4, /*56*/6, /*57*/2,
 /*58*/2, /*59*/4, /*5A*/2, /*5B*/2,  /*5C*/2, /*5D*/4, /*5E*/7, /*5F*/2,

/*60*/6, /*61*/6, /*62*/2, /*63*/2,  /*64*/2, /*65*/3, /*66*/5, /*67*/2,
 /*68*/4, /*69*/2, /*6A*/2, /*6B*/2,  /*6C*/5, /*6D*/4, /*6E*/6, /*6F*/2,

/*70*/2, /*71*/5, /*72*/2, /*73*/2,  /*74*/2, /*75*/4, /*76*/6, /*77*/2,
 /*78*/2, /*79*/4, /*7A*/2, /*7B*/2,  /*7C*/2, /*7D*/4, /*7E*/7, /*7F*/2,

/*80*/2, /*81*/6, /*82*/2, /*83*/2,  /*84*/3, /*85*/3, /*86*/3, /*87*/2,
 /*88*/2, /*89*/2, /*8A*/2, /*8B*/2,  /*8C*/4, /*8D*/4, /*8E*/4, /*8F*/2,

/*90*/2, /*91*/6, /*92*/2, /*93*/2,  /*94*/4, /*95*/4, /*96*/4, /*97*/2,
 /*98*/2, /*99*/5, /*9A*/2, /*9B*/2,  /*9C*/2, /*9D*/5, /*9E*/2, /*9F*/2,

/*A0*/2, /*A1*/6, /*A2*/2, /*A3*/2,  /*A4*/3, /*A5*/3, /*A6*/3, /*A7*/2,
 /*A8*/2, /*A9*/2, /*AA*/2, /*AB*/2,  /*AC*/4, /*AD*/4, /*AE*/4, /*AF*/2,

/*B0*/2, /*B1*/5, /*B2*/2, /*B3*/2,  /*B4*/4, /*B5*/4, /*B6*/4, /*B7*/2,
 /*B8*/2, /*B9*/4, /*BA*/2, /*BB*/2,  /*BC*/4, /*BD*/4, /*BE*/4, /*BF*/2,

/*C0*/2, /*C1*/6, /*C2*/2, /*C3*/2,  /*C4*/3, /*C5*/3, /*C6*/5, /*C7*/2,
 /*C8*/2, /*C9*/2, /*CA*/2, /*CB*/2,  /*CC*/4, /*CD*/4, /*CE*/6, /*CF*/2,

/*D0*/2, /*D1*/5, /*D2*/2, /*D3*/2,  /*D4*/2, /*D5*/4, /*D6*/6, /*D7*/2,
 /*D8*/2, /*D9*/4, /*DA*/2, /*DB*/2,  /*DC*/2, /*DD*/4, /*DE*/7, /*DF*/2,

/*E0*/2, /*E1*/6, /*E2*/2, /*E3*/2,  /*E4*/3, /*E5*/3, /*E6*/5, /*E7*/2,
 /*E8*/2, /*E9*/2, /*EA*/2, /*EB*/2,  /*EC*/4, /*ED*/4, /*EE*/6, /*EF*/2,

/*F0*/2, /*F1*/5, /*F2*/2, /*F3*/2,  /*F4*/2, /*F5*/4, /*F6*/6, /*F7*/2,
 /*F8*/2, /*F9*/4, /*FA*/2, /*FB*/2,  /*FC*/2, /*FD*/4, /*FE*/7, /*FF*/2,
};

// -----------------------------------------------------------------------------
// Instruction lengths (bytes) per opcode
// -----------------------------------------------------------------------------
const uint8_t cpu_instr_len[256] = {
/*00*/1,/*01*/2,/*02*/1,/*03*/1, /*04*/2,/*05*/2,/*06*/2,/*07*/1,
 /*08*/1,/*09*/2,/*0A*/1,/*0B*/1, /*0C*/3,/*0D*/3,/*0E*/3,/*0F*/1,

/*10*/2,/*11*/2,/*12*/1,/*13*/1, /*14*/2,/*15*/2,/*16*/2,/*17*/1,
 /*18*/1,/*19*/3,/*1A*/1,/*1B*/1, /*1C*/3,/*1D*/3,/*1E*/3,/*1F*/1,

/*20*/3,/*21*/2,/*22*/1,/*23*/1, /*24*/2,/*25*/2,/*26*/2,/*27*/1,
 /*28*/1,/*29*/2,/*2A*/1,/*2B*/1, /*2C*/3,/*2D*/3,/*2E*/3,/*2F*/1,

/*30*/2,/*31*/2,/*32*/1,/*33*/1, /*34*/2,/*35*/2,/*36*/2,/*37*/1,
 /*38*/1,/*39*/3,/*3A*/1,/*3B*/1, /*3C*/3,/*3D*/3,/*3E*/3,/*3F*/1,

/*40*/1,/*41*/2,/*42*/1,/*43*/1, /*44*/2,/*45*/2,/*46*/2,/*47*/1,
 /*48*/1,/*49*/2,/*4A*/1,/*4B*/1, /*4C*/3,/*4D*/3,/*4E*/3,/*4F*/1,

/*50*/2,/*51*/2,/*52*/1,/*53*/1, /*54*/2,/*55*/2,/*56*/2,/*57*/1,
 /*58*/1,/*59*/3,/*5A*/1,/*5B*/1, /*5C*/3,/*5D*/3,/*5E*/3,/*5F*/1,

/*60*/1,/*61*/2,/*62*/1,/*63*/1, /*64*/2,/*65*/2,/*66*/2,/*67*/1,
 /*68*/1,/*69*/2,/*6A*/1,/*6B*/1, /*6C*/3,/*6D*/3,/*6E*/3,/*6F*/1,

/*70*/2,/*71*/2,/*72*/1,/*73*/1, /*74*/2,/*75*/2,/*76*/2,/*77*/1,
 /*78*/1,/*79*/3,/*7A*/1,/*7B*/1, /*7C*/3,/*7D*/3,/*7E*/3,/*7F*/1,

/*80*/2,/*81*/2,/*82*/2,/*83*/1, /*84*/2,/*85*/2,/*86*/2,/*87*/1,
 /*88*/1,/*89*/2,/*8A*/1,/*8B*/1, /*8C*/3,/*8D*/3,/*8E*/3,/*8F*/1,

/*90*/2,/*91*/2,/*92*/1,/*93*/1, /*94*/2,/*95*/2,/*96*/2,/*97*/1,
 /*98*/1,/*99*/3,/*9A*/1,/*9B*/1, /*9C*/3,/*9D*/3,/*9E*/3,/*9F*/1,

/*A0*/2,/*A1*/2,/*A2*/2,/*A3*/1, /*A4*/2,/*A5*/2,/*A6*/2,/*A7*/1,
 /*A8*/1,/*A9*/2,/*AA*/1,/*AB*/1, /*AC*/3,/*AD*/3,/*AE*/3,/*AF*/1,

/*B0*/2,/*B1*/2,/*B2*/1,/*B3*/1, /*B4*/2,/*B5*/2,/*B6*/2,/*B7*/1,
 /*B8*/1,/*B9*/3,/*BA*/1,/*BB*/1, /*BC*/3,/*BD*/3,/*BE*/3,/*BF*/1,

/*C0*/2,/*C1*/2,/*C2*/1,/*C3*/1, /*C4*/2,/*C5*/2,/*C6*/2,/*C7*/1,
 /*C8*/1,/*C9*/2,/*CA*/1,/*CB*/1, /*CC*/3,/*CD*/3,/*CE*/3,/*CF*/1,

/*D0*/2,/*D1*/2,/*D2*/1,/*D3*/1, /*D4*/2,/*D5*/2,/*D6*/2,/*D7*/1,
 /*D8*/1,/*D9*/3,/*DA*/1,/*DB*/1, /*DC*/3,/*DD*/3,/*DE*/3,/*DF*/1,

/*E0*/2,/*E1*/2,/*E2*/1,/*E3*/1, /*E4*/2,/*E5*/2,/*E6*/2,/*E7*/1,
 /*E8*/1,/*E9*/2,/*EA*/1,/*EB*/1, /*EC*/3,/*ED*/3,/*EE*/3,/*EF*/1,

/*F0*/2,/*F1*/2,/*F2*/1,/*F3*/1, /*F4*/2,/*F5*/2,/*F6*/2,/*F7*/1,
 /*F8*/1,/*F9*/3,/*FA*/1,/*FB*/1, /*FC*/3,/*FD*/3,/*FE*/3,/*FF*/1,
};

// -----------------------------------------------------------------------------
// Mnemonics & addressing mode strings
// For illegal opcodes we use "???"/"-".
// -----------------------------------------------------------------------------
#define MN(x) x
#define AM(x) x

const char* const cpu_mnemonic[256] = {
/*00*/MN("BRK"),/*01*/MN("ORA"),/*02*/MN("???"),/*03*/MN("???"),
/*04*/MN("???"),/*05*/MN("ORA"),/*06*/MN("ASL"),/*07*/MN("???"),
/*08*/MN("PHP"),/*09*/MN("ORA"),/*0A*/MN("ASL"),/*0B*/MN("???"),
/*0C*/MN("???"),/*0D*/MN("ORA"),/*0E*/MN("ASL"),/*0F*/MN("???"),

/*10*/MN("BPL"),/*11*/MN("ORA"),/*12*/MN("???"),/*13*/MN("???"),
/*14*/MN("???"),/*15*/MN("ORA"),/*16*/MN("ASL"),/*17*/MN("???"),
/*18*/MN("CLC"),/*19*/MN("ORA"),/*1A*/MN("???"),/*1B*/MN("???"),
/*1C*/MN("???"),/*1D*/MN("ORA"),/*1E*/MN("ASL"),/*1F*/MN("???"),

/*20*/MN("JSR"),/*21*/MN("AND"),/*22*/MN("???"),/*23*/MN("???"),
/*24*/MN("BIT"),/*25*/MN("AND"),/*26*/MN("ROL"),/*27*/MN("???"),
/*28*/MN("PLP"),/*29*/MN("AND"),/*2A*/MN("ROL"),/*2B*/MN("???"),
/*2C*/MN("BIT"),/*2D*/MN("AND"),/*2E*/MN("ROL"),/*2F*/MN("???"),

/*30*/MN("BMI"),/*31*/MN("AND"),/*32*/MN("???"),/*33*/MN("???"),
/*34*/MN("???"),/*35*/MN("AND"),/*36*/MN("ROL"),/*37*/MN("???"),
/*38*/MN("SEC"),/*39*/MN("AND"),/*3A*/MN("???"),/*3B*/MN("???"),
/*3C*/MN("???"),/*3D*/MN("AND"),/*3E*/MN("ROL"),/*3F*/MN("???"),

/*40*/MN("RTI"),/*41*/MN("EOR"),/*42*/MN("???"),/*43*/MN("???"),
/*44*/MN("???"),/*45*/MN("EOR"),/*46*/MN("LSR"),/*47*/MN("???"),
/*48*/MN("PHA"),/*49*/MN("EOR"),/*4A*/MN("LSR"),/*4B*/MN("???"),
/*4C*/MN("JMP"),/*4D*/MN("EOR"),/*4E*/MN("LSR"),/*4F*/MN("???"),

/*50*/MN("BVC"),/*51*/MN("EOR"),/*52*/MN("???"),/*53*/MN("???"),
/*54*/MN("???"),/*55*/MN("EOR"),/*56*/MN("LSR"),/*57*/MN("???"),
/*58*/MN("CLI"),/*59*/MN("EOR"),/*5A*/MN("???"),/*5B*/MN("???"),
/*5C*/MN("???"),/*5D*/MN("EOR"),/*5E*/MN("LSR"),/*5F*/MN("???"),

/*60*/MN("RTS"),/*61*/MN("ADC"),/*62*/MN("???"),/*63*/MN("???"),
/*64*/MN("???"),/*65*/MN("ADC"),/*66*/MN("ROR"),/*67*/MN("???"),
/*68*/MN("PLA"),/*69*/MN("ADC"),/*6A*/MN("ROR"),/*6B*/MN("???"),
/*6C*/MN("JMP"),/*6D*/MN("ADC"),/*6E*/MN("ROR"),/*6F*/MN("???"),

/*70*/MN("BVS"),/*71*/MN("ADC"),/*72*/MN("???"),/*73*/MN("???"),
/*74*/MN("???"),/*75*/MN("ADC"),/*76*/MN("ROR"),/*77*/MN("???"),
/*78*/MN("SEI"),/*79*/MN("ADC"),/*7A*/MN("???"),/*7B*/MN("???"),
/*7C*/MN("???"),/*7D*/MN("ADC"),/*7E*/MN("ROR"),/*7F*/MN("???"),

/*80*/MN("???"),/*81*/MN("STA"),/*82*/MN("???"),/*83*/MN("???"),
/*84*/MN("STY"),/*85*/MN("STA"),/*86*/MN("STX"),/*87*/MN("???"),
/*88*/MN("DEY"),/*89*/MN("???"),/*8A*/MN("TXA"),/*8B*/MN("???"),
/*8C*/MN("STY"),/*8D*/MN("STA"),/*8E*/MN("STX"),/*8F*/MN("???"),

/*90*/MN("BCC"),/*91*/MN("STA"),/*92*/MN("???"),/*93*/MN("???"),
/*94*/MN("STY"),/*95*/MN("STA"),/*96*/MN("STX"),/*97*/MN("???"),
/*98*/MN("TYA"),/*99*/MN("STA"),/*9A*/MN("TXS"),/*9B*/MN("???"),
/*9C*/MN("???"),/*9D*/MN("STA"),/*9E*/MN("???"),/*9F*/MN("???"),

/*A0*/MN("LDY"),/*A1*/MN("LDA"),/*A2*/MN("LDX"),/*A3*/MN("???"),
/*A4*/MN("LDY"),/*A5*/MN("LDA"),/*A6*/MN("LDX"),/*A7*/MN("???"),
/*A8*/MN("TAY"),/*A9*/MN("LDA"),/*AA*/MN("TAX"),/*AB*/MN("???"),
/*AC*/MN("LDY"),/*AD*/MN("LDA"),/*AE*/MN("LDX"),/*AF*/MN("???"),

/*B0*/MN("BCS"),/*B1*/MN("LDA"),/*B2*/MN("???"),/*B3*/MN("???"),
/*B4*/MN("LDY"),/*B5*/MN("LDA"),/*B6*/MN("LDX"),/*B7*/MN("???"),
/*B8*/MN("CLV"),/*B9*/MN("LDA"),/*BA*/MN("TSX"),/*BB*/MN("???"),
/*BC*/MN("LDY"),/*BD*/MN("LDA"),/*BE*/MN("LDX"),/*BF*/MN("???"),

/*C0*/MN("CPY"),/*C1*/MN("CMP"),/*C2*/MN("???"),/*C3*/MN("???"),
/*C4*/MN("CPY"),/*C5*/MN("CMP"),/*C6*/MN("DEC"),/*C7*/MN("???"),
/*C8*/MN("INY"),/*C9*/MN("CMP"),/*CA*/MN("DEX"),/*CB*/MN("???"),
/*CC*/MN("CPY"),/*CD*/MN("CMP"),/*CE*/MN("DEC"),/*CF*/MN("???"),

/*D0*/MN("BNE"),/*D1*/MN("CMP"),/*D2*/MN("???"),/*D3*/MN("???"),
/*D4*/MN("???"),/*D5*/MN("CMP"),/*D6*/MN("DEC"),/*D7*/MN("???"),
/*D8*/MN("CLD"),/*D9*/MN("CMP"),/*DA*/MN("???"),/*DB*/MN("???"),
/*DC*/MN("???"),/*DD*/MN("CMP"),/*DE*/MN("DEC"),/*DF*/MN("???"),

/*E0*/MN("CPX"),/*E1*/MN("SBC"),/*E2*/MN("???"),/*E3*/MN("???"),
/*E4*/MN("CPX"),/*E5*/MN("SBC"),/*E6*/MN("INC"),/*E7*/MN("???"),
/*E8*/MN("INX"),/*E9*/MN("SBC"),/*EA*/MN("NOP"),/*EB*/MN("???"),
/*EC*/MN("CPX"),/*ED*/MN("SBC"),/*EE*/MN("INC"),/*EF*/MN("???"),

/*F0*/MN("BEQ"),/*F1*/MN("SBC"),/*F2*/MN("???"),/*F3*/MN("???"),
/*F4*/MN("???"),/*F5*/MN("SBC"),/*F6*/MN("INC"),/*F7*/MN("???"),
/*F8*/MN("SED"),/*F9*/MN("SBC"),/*FA*/MN("???"),/*FB*/MN("???"),
/*FC*/MN("???"),/*FD*/MN("SBC"),/*FE*/MN("INC"),/*FF*/MN("???"),
};

const char* const cpu_addrmode[256] = {
/*00*/AM("impl"), /*01*/AM("(ind,X)"),/*02*/AM("-"),    /*03*/AM("-"),
/*04*/AM("zp"),   /*05*/AM("zp"),     /*06*/AM("zp"),   /*07*/AM("-"),
/*08*/AM("impl"), /*09*/AM("#imm"),   /*0A*/AM("A"),    /*0B*/AM("-"),
/*0C*/AM("abs"),  /*0D*/AM("abs"),    /*0E*/AM("abs"),  /*0F*/AM("-"),

/*10*/AM("rel"),  /*11*/AM("(ind),Y"),/*12*/AM("-"),    /*13*/AM("-"),
/*14*/AM("zp,X"), /*15*/AM("zp,X"),   /*16*/AM("zp,X"), /*17*/AM("-"),
/*18*/AM("impl"), /*19*/AM("abs,Y"),  /*1A*/AM("impl"), /*1B*/AM("-"),
/*1C*/AM("abs,X"),/*1D*/AM("abs,X"),  /*1E*/AM("abs,X"),/*1F*/AM("-"),

/*20*/AM("abs"),  /*21*/AM("(ind,X)"),/*22*/AM("-"),    /*23*/AM("-"),
/*24*/AM("zp"),   /*25*/AM("zp"),     /*26*/AM("zp"),   /*27*/AM("-"),
/*28*/AM("impl"), /*29*/AM("#imm"),   /*2A*/AM("A"),    /*2B*/AM("-"),
/*2C*/AM("abs"),  /*2D*/AM("abs"),    /*2E*/AM("abs"),  /*2F*/AM("-"),

/*30*/AM("rel"),  /*31*/AM("(ind),Y"),/*32*/AM("-"),    /*33*/AM("-"),
/*34*/AM("zp,X"), /*35*/AM("zp,X"),   /*36*/AM("zp,X"), /*37*/AM("-"),
/*38*/AM("impl"), /*39*/AM("abs,Y"),  /*3A*/AM("impl"), /*3B*/AM("-"),
/*3C*/AM("abs,X"),/*3D*/AM("abs,X"),  /*3E*/AM("abs,X"),/*3F*/AM("-"),

/*40*/AM("impl"), /*41*/AM("(ind,X)"),/*42*/AM("-"),    /*43*/AM("-"),
/*44*/AM("zp"),   /*45*/AM("zp"),     /*46*/AM("zp"),   /*47*/AM("-"),
/*48*/AM("impl"), /*49*/AM("#imm"),   /*4A*/AM("A"),    /*4B*/AM("-"),
/*4C*/AM("abs"),  /*4D*/AM("abs"),    /*4E*/AM("abs"),  /*4F*/AM("-"),

/*50*/AM("rel"),  /*51*/AM("(ind),Y"),/*52*/AM("-"),    /*53*/AM("-"),
/*54*/AM("zp,X"), /*55*/AM("zp,X"),   /*56*/AM("zp,X"), /*57*/AM("-"),
/*58*/AM("impl"), /*59*/AM("abs,Y"),  /*5A*/AM("impl"), /*5B*/AM("-"),
/*5C*/AM("abs,X"),/*5D*/AM("abs,X"),  /*5E*/AM("abs,X"),/*5F*/AM("-"),

/*60*/AM("impl"), /*61*/AM("(ind,X)"),/*62*/AM("-"),    /*63*/AM("-"),
/*64*/AM("zp"),   /*65*/AM("zp"),     /*66*/AM("zp"),   /*67*/AM("-"),
/*68*/AM("impl"), /*69*/AM("#imm"),   /*6A*/AM("A"),    /*6B*/AM("-"),
/*6C*/AM("(ind)"),/*6D*/AM("abs"),    /*6E*/AM("abs"),  /*6F*/AM("-"),

/*70*/AM("rel"),  /*71*/AM("(ind),Y"),/*72*/AM("-"),    /*73*/AM("-"),
/*74*/AM("zp,X"), /*75*/AM("zp,X"),   /*76*/AM("zp,X"), /*77*/AM("-"),
/*78*/AM("impl"), /*79*/AM("abs,Y"),  /*7A*/AM("impl"), /*7B*/AM("-"),
/*7C*/AM("abs,X"),/*7D*/AM("abs,X"),  /*7E*/AM("abs,X"),/*7F*/AM("-"),

/*80*/AM("#imm"), /*81*/AM("(ind,X)"),/*82*/AM("#imm"), /*83*/AM("-"),
/*84*/AM("zp"),   /*85*/AM("zp"),     /*86*/AM("zp"),   /*87*/AM("-"),
/*88*/AM("impl"), /*89*/AM("#imm"),   /*8A*/AM("impl"), /*8B*/AM("-"),
/*8C*/AM("abs"),  /*8D*/AM("abs"),    /*8E*/AM("abs"),  /*8F*/AM("-"),

/*90*/AM("rel"),  /*91*/AM("(ind),Y"),/*92*/AM("-"),    /*93*/AM("-"),
/*94*/AM("zp,X"), /*95*/AM("zp,X"),   /*96*/AM("zp,Y"), /*97*/AM("-"),
/*98*/AM("impl"), /*99*/AM("abs,Y"),  /*9A*/AM("impl"), /*9B*/AM("-"),
/*9C*/AM("abs"),  /*9D*/AM("abs,X"),  /*9E*/AM("abs"),  /*9F*/AM("-"),

/*A0*/AM("#imm"), /*A1*/AM("(ind,X)"),/*A2*/AM("#imm"), /*A3*/AM("-"),
/*A4*/AM("zp"),   /*A5*/AM("zp"),     /*A6*/AM("zp"),   /*A7*/AM("-"),
/*A8*/AM("impl"), /*A9*/AM("#imm"),   /*AA*/AM("impl"), /*AB*/AM("-"),
/*AC*/AM("abs"),  /*AD*/AM("abs"),    /*AE*/AM("abs"),  /*AF*/AM("-"),

/*B0*/AM("rel"),  /*B1*/AM("(ind),Y"),/*B2*/AM("-"),    /*B3*/AM("-"),
/*B4*/AM("zp,X"), /*B5*/AM("zp,X"),   /*B6*/AM("zp,Y"), /*B7*/AM("-"),
/*B8*/AM("impl"), /*B9*/AM("abs,Y"),  /*BA*/AM("impl"), /*BB*/AM("-"),
/*BC*/AM("abs,X"),/*BD*/AM("abs,X"),  /*BE*/AM("abs,Y"),/*BF*/AM("-"),

/*C0*/AM("#imm"), /*C1*/AM("(ind,X)"),/*C2*/AM("-"),    /*C3*/AM("-"),
/*C4*/AM("zp"),   /*C5*/AM("zp"),     /*C6*/AM("zp"),   /*C7*/AM("-"),
/*C8*/AM("impl"), /*C9*/AM("#imm"),   /*CA*/AM("impl"), /*CB*/AM("-"),
/*CC*/AM("abs"),  /*CD*/AM("abs"),    /*CE*/AM("abs"),  /*CF*/AM("-"),

/*D0*/AM("rel"),  /*D1*/AM("(ind),Y"),/*D2*/AM("-"),    /*D3*/AM("-"),
/*D4*/AM("zp,X"), /*D5*/AM("zp,X"),   /*D6*/AM("zp,X"), /*D7*/AM("-"),
/*D8*/AM("impl"), /*D9*/AM("abs,Y"),  /*DA*/AM("impl"), /*DB*/AM("-"),
/*DC*/AM("abs,X"),/*DD*/AM("abs,X"),  /*DE*/AM("abs,X"),/*DF*/AM("-"),

/*E0*/AM("#imm"), /*E1*/AM("(ind,X)"),/*E2*/AM("-"),    /*E3*/AM("-"),
/*E4*/AM("zp"),   /*E5*/AM("zp"),     /*E6*/AM("zp"),   /*E7*/AM("-"),
/*E8*/AM("impl"), /*E9*/AM("#imm"),   /*EA*/AM("impl"), /*EB*/AM("-"),
/*EC*/AM("abs"),  /*ED*/AM("abs"),    /*EE*/AM("abs"),  /*EF*/AM("-"),

/*F0*/AM("rel"),  /*F1*/AM("(ind),Y"),/*F2*/AM("-"),    /*F3*/AM("-"),
/*F4*/AM("zp,X"), /*F5*/AM("zp,X"),   /*F6*/AM("zp,X"), /*F7*/AM("-"),
/*F8*/AM("impl"), /*F9*/AM("abs,Y"),  /*FA*/AM("impl"), /*FB*/AM("-"),
/*FC*/AM("abs,X"),/*FD*/AM("abs,X"),  /*FE*/AM("abs,X"),/*FF*/AM("-"),
};
