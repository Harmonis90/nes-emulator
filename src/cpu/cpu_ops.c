// cpu_ops.c — 6502 opcode implementations (base cycles removed; only conditional adds remain)

#include <stdint.h>
#include "cpu.h"
#include "bus.h"
#include "cpu_internal.h"
#include "cpu_addr.h"
#include "cpu_ops.h"

// Only used for conditional extras (branches/page-cross). Base cycles are added in cpu_step().
#define ADD_CYC(n) cpu_cycles_add((n))

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------
static inline uint8_t rd(uint16_t a) { return cpu_read(a); }
static inline void     wr(uint16_t a, uint8_t v) { cpu_write(a, v); }

static inline void lda_load(uint8_t v){ cpu_set_a(v); set_zn(v); }
static inline void ldx_load(uint8_t v){ cpu_set_x(v); set_zn(v); }
static inline void ldy_load(uint8_t v){ cpu_set_y(v); set_zn(v); }

static inline void ora_do(uint8_t m){ uint8_t a=(uint8_t)(cpu_get_a()|m); cpu_set_a(a); set_zn(a); }
static inline void and_do(uint8_t m){ uint8_t a=(uint8_t)(cpu_get_a()&m); cpu_set_a(a); set_zn(a); }
static inline void eor_do(uint8_t m){ uint8_t a=(uint8_t)(cpu_get_a()^m); cpu_set_a(a); set_zn(a); }

static inline void cmp_do(uint8_t r, uint8_t m){
    uint16_t t = (uint16_t)r - (uint16_t)m;
    set_flag(FLAG_C, r >= m);
    set_zn((uint8_t)t);
}

// ADC/SBC
void do_adc(uint8_t m){
    uint16_t a = cpu_get_a();
    uint16_t c = get_flag(FLAG_C);
    uint16_t sum = a + m + c;
    set_flag(FLAG_C, sum > 0xFF);
    set_flag(FLAG_V, (~(a ^ m) & (a ^ sum) & 0x80) != 0);
    cpu_set_a((uint8_t)sum);
    set_zn(cpu_get_a());
}
static inline void do_sbc(uint8_t m){ do_adc((uint8_t)(m ^ 0xFF)); }

// RMW on accumulator
static inline uint8_t asl_val(uint8_t v){ set_flag(FLAG_C, (v & 0x80)!=0); v<<=1; set_zn(v); return v; }
static inline uint8_t lsr_val(uint8_t v){ set_flag(FLAG_C, (v & 0x01)!=0); v>>=1; set_zn(v); return v; }
static inline uint8_t rol_val(uint8_t v){
    uint8_t c = (uint8_t)get_flag(FLAG_C);
    set_flag(FLAG_C, (v & 0x80)!=0);
    v = (uint8_t)((v<<1) | c);
    set_zn(v);
    return v;
}
static inline uint8_t ror_val(uint8_t v){
    uint8_t c = (uint8_t)get_flag(FLAG_C);
    set_flag(FLAG_C, (v & 0x01)!=0);
    v = (uint8_t)((v>>1) | (c<<7));
    set_zn(v);
    return v;
}

// RMW writeback helpers (no base cycle adds here; base is in the table)
static inline void rmw_zp (uint8_t zp, uint8_t (*fn)(uint8_t)){
    uint16_t a = (uint16_t)zp;
    uint8_t v = rd(a); v = fn(v); wr(a, v);
}
static inline void rmw_zpx(uint8_t zp, uint8_t (*fn)(uint8_t)){
    uint8_t a8 = (uint8_t)(zp + cpu_get_x());
    uint16_t a = (uint16_t)a8;
    uint8_t v = rd(a); v = fn(v); wr(a, v);
}
static inline void rmw_abs(uint16_t addr, uint8_t (*fn)(uint8_t)){
    uint8_t v = rd(addr); v = fn(v); wr(addr, v);
}
static inline void rmw_abx(uint16_t addr, uint8_t (*fn)(uint8_t)){
    uint16_t a = (uint16_t)(addr + cpu_get_x());
    uint8_t v = rd(a); v = fn(v); wr(a, v);
}

// -----------------------------------------------------------------------------
// Branch helper: +1 if taken; +1 more if page crossed
// (Base=2 is added by cpu_step via cpu_base_cycles)
// -----------------------------------------------------------------------------
// in cpu_ops.c
static inline void branch_if(int cond) {
    int8_t rel = (int8_t)fetch8();            // PC now points to next instr
    uint16_t pc_after = cpu_get_pc();
    if (cond) {
        uint16_t tgt = (uint16_t)(pc_after + rel);
        TRACE("BR   rel=%d  pc_after=%04X -> tgt=%04X  crossed=%d\n",
              rel, pc_after, tgt, ((pc_after ^ tgt) & 0xFF00) != 0);
        cpu_set_pc(tgt);
        cpu_cycles_add(1);
        if (((pc_after ^ tgt) & 0xFF00) != 0) cpu_cycles_add(1);
    } else {
        TRACE("BR   not-taken  rel=%d  pc_after=%04X\n", rel, pc_after);
    }
}


// -----------------------------------------------------------------------------
// SYSTEM / FLOW
// (No base adds here: base cycle counts are in cpu_base_cycles)
// -----------------------------------------------------------------------------
void BRK(void){
    (void)fetch8();               // padding byte
    interrupt_enter(VEC_IRQ_BRK, 1);
}
void RTI(void){
    uint8_t p = pop8();
    p &= (uint8_t)~FLAG_B; p |= FLAG_U;
    cpu_set_p(p);
    cpu_set_pc(pop16());
}
void RTS(void){
    uint16_t ret = pop16();
    cpu_set_pc((uint16_t)(ret + 1));
}
void JSR_abs(void){
    uint16_t dst = fetch16();
#ifdef DEBUG_TRACE
    fprintf(stderr, "JSR to %04X  bytes: [%04X]=%02X  [%04X]=%02X  [%04X]=%02X\n",
            dst,
            (uint16_t)(dst-1), cpu_read((uint16_t)(dst-1)),
            dst,             cpu_read(dst),
            (uint16_t)(dst+1), cpu_read((uint16_t)(dst+1)));
#endif
    uint16_t ret = (uint16_t)(cpu_get_pc() - 1);
    push16(ret);
    cpu_set_pc(dst);
}

void JMP_abs(void){ cpu_set_pc(fetch16()); }
void JMP_ind(void){ cpu_set_pc(cpu_addr_ind()); }

// -----------------------------------------------------------------------------
// BRANCHES
// -----------------------------------------------------------------------------
void BPL(void) { branch_if((get_flag(FLAG_N) == 0)); }
void BMI(void) { branch_if((get_flag(FLAG_N) != 0)); }
void BVC(void) { branch_if((get_flag(FLAG_V) == 0)); }
void BVS(void) { branch_if((get_flag(FLAG_V) != 0)); }
void BCC(void) { branch_if((get_flag(FLAG_C) == 0)); }
void BCS(void) { branch_if((get_flag(FLAG_C) != 0)); }
void BNE(void) { branch_if((get_flag(FLAG_Z) == 0)); }
void BEQ(void) { branch_if((get_flag(FLAG_Z) != 0)); }

// -----------------------------------------------------------------------------
// LOADS
// -----------------------------------------------------------------------------
void LDA_imm(void){

    uint16_t pc_before = cpu_get_pc();
    uint8_t v = fetch8();
    TRACE("LDA# imm=%02X  at=%04X\n", v, pc_before);
    cpu_set_a(v);
    set_zn(v);
}

void LDA_zp (void){ lda_load(rd(cpu_addr_zp())); }
void LDA_zpx(void){ lda_load(rd(cpu_addr_zpx())); }
void LDA_abs(void){ lda_load(rd(cpu_addr_abs())); }
void LDA_abx(void){
    eff_t e = cpu_addr_abx(); lda_load(rd(e.addr));
    if(e.crossed) ADD_CYC(1);
}
void LDA_aby(void){
    eff_t e = cpu_addr_aby(); lda_load(rd(e.addr));
    if(e.crossed) ADD_CYC(1);
}
void LDA_inx(void){ lda_load(rd(cpu_addr_inx())); }
void LDA_iny(void){
    eff_t e = cpu_addr_iny(); lda_load(rd(e.addr));
    if(e.crossed) ADD_CYC(1);
}

void LDX_imm(void){ ldx_load(fetch8()); }
void LDX_zp (void){ ldx_load(rd(cpu_addr_zp())); }
void LDX_zpy(void){ ldx_load(rd(cpu_addr_zpy())); }
void LDX_abs(void){ ldx_load(rd(cpu_addr_abs())); }
void LDX_aby(void){
    eff_t e = cpu_addr_aby(); ldx_load(rd(e.addr));
    if(e.crossed) ADD_CYC(1);
}

void LDY_imm(void){ ldy_load(fetch8()); }
void LDY_zp (void){ ldy_load(rd(cpu_addr_zp())); }
void LDY_zpx(void){ ldy_load(rd(cpu_addr_zpx())); }
void LDY_abs(void){ ldy_load(rd(cpu_addr_abs())); }
void LDY_abx(void){
    eff_t e = cpu_addr_abx(); ldy_load(rd(e.addr));
    if(e.crossed) ADD_CYC(1);
}

// -----------------------------------------------------------------------------
// STORES (no page-cross bonus on stores)
// -----------------------------------------------------------------------------
void STA_zp (void){ wr(cpu_addr_zp(),  cpu_get_a()); }
void STA_zpx(void){ wr(cpu_addr_zpx(), cpu_get_a()); }
void STA_abs(void){ wr(cpu_addr_abs(), cpu_get_a()); }
void STA_abx(void){ wr((uint16_t)(cpu_addr_abs()+cpu_get_x()), cpu_get_a()); }
void STA_aby(void){ wr((uint16_t)(cpu_addr_abs()+cpu_get_y()), cpu_get_a()); }
void STA_inx(void){ wr(cpu_addr_inx(), cpu_get_a()); }
void STA_iny(void){ wr(cpu_addr_iny().addr, cpu_get_a()); }

void STX_zp (void){ wr(cpu_addr_zp(),  cpu_get_x()); }
void STX_zpy(void){ wr(cpu_addr_zpy(), cpu_get_x()); }
void STX_abs(void){ wr(cpu_addr_abs(), cpu_get_x()); }

void STY_zp (void){ wr(cpu_addr_zp(),  cpu_get_y()); }
void STY_zpx(void){ wr(cpu_addr_zpx(), cpu_get_y()); }
void STY_abs(void){ wr(cpu_addr_abs(), cpu_get_y()); }

// -----------------------------------------------------------------------------
// LOGICAL (ORA/AND/EOR)
// -----------------------------------------------------------------------------
void ORA_imm(void){ ora_do(fetch8()); }
void ORA_zp (void){ ora_do(rd(cpu_addr_zp())); }
void ORA_zpx(void){ ora_do(rd(cpu_addr_zpx())); }
void ORA_abs(void){ ora_do(rd(cpu_addr_abs())); }
void ORA_abx(void){
    eff_t e = cpu_addr_abx(); ora_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void ORA_aby(void){
    eff_t e = cpu_addr_aby(); ora_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void ORA_inx(void){ ora_do(rd(cpu_addr_inx())); }
void ORA_iny(void){
    eff_t e = cpu_addr_iny(); ora_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}

void AND_imm(void){ and_do(fetch8()); }
void AND_zp (void){ and_do(rd(cpu_addr_zp())); }
void AND_zpx(void){ and_do(rd(cpu_addr_zpx())); }
void AND_abs(void){ and_do(rd(cpu_addr_abs())); }
void AND_abx(void){
    eff_t e = cpu_addr_abx(); and_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void AND_aby(void){
    eff_t e = cpu_addr_aby(); and_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void AND_inx(void){ and_do(rd(cpu_addr_inx())); }
void AND_iny(void){
    eff_t e = cpu_addr_iny(); and_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}

void EOR_imm(void){ eor_do(fetch8()); }
void EOR_zp (void){ eor_do(rd(cpu_addr_zp())); }
void EOR_zpx(void){ eor_do(rd(cpu_addr_zpx())); }
void EOR_abs(void){ eor_do(rd(cpu_addr_abs())); }
void EOR_abx(void){
    eff_t e = cpu_addr_abx(); eor_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void EOR_aby(void){
    eff_t e = cpu_addr_aby(); eor_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void EOR_inx(void){ eor_do(rd(cpu_addr_inx())); }
void EOR_iny(void){
    eff_t e = cpu_addr_iny(); eor_do(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}

// -----------------------------------------------------------------------------
// ADC / SBC
// -----------------------------------------------------------------------------
void ADC_imm(void){ do_adc(fetch8()); }
void ADC_zp (void){ do_adc(rd(cpu_addr_zp())); }
void ADC_zpx(void){ do_adc(rd(cpu_addr_zpx())); }
void ADC_abs(void){ do_adc(rd(cpu_addr_abs())); }
void ADC_abx(void){
    eff_t e = cpu_addr_abx(); do_adc(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void ADC_aby(void){
    eff_t e = cpu_addr_aby(); do_adc(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void ADC_inx(void){ do_adc(rd(cpu_addr_inx())); }
void ADC_iny(void){
    eff_t e = cpu_addr_iny(); do_adc(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}

void SBC_imm(void){ do_sbc(fetch8()); }
void SBC_zp (void){ do_sbc(rd(cpu_addr_zp())); }
void SBC_zpx(void){ do_sbc(rd(cpu_addr_zpx())); }
void SBC_abs(void){ do_sbc(rd(cpu_addr_abs())); }
void SBC_abx(void){
    eff_t e = cpu_addr_abx(); do_sbc(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void SBC_aby(void){
    eff_t e = cpu_addr_aby(); do_sbc(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void SBC_inx(void){ do_sbc(rd(cpu_addr_inx())); }
void SBC_iny(void){
    eff_t e = cpu_addr_iny(); do_sbc(rd(e.addr)); if(e.crossed) ADD_CYC(1);
}

// -----------------------------------------------------------------------------
// COMPARES
// -----------------------------------------------------------------------------
void CMP_imm(void){ cmp_do(cpu_get_a(), fetch8()); }
void CMP_zp (void){ cmp_do(cpu_get_a(), rd(cpu_addr_zp())); }
void CMP_zpx(void){ cmp_do(cpu_get_a(), rd(cpu_addr_zpx())); }
void CMP_abs(void){ cmp_do(cpu_get_a(), rd(cpu_addr_abs())); }
void CMP_abx(void){
    eff_t e = cpu_addr_abx(); cmp_do(cpu_get_a(), rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void CMP_aby(void){
    eff_t e = cpu_addr_aby(); cmp_do(cpu_get_a(), rd(e.addr)); if(e.crossed) ADD_CYC(1);
}
void CMP_inx(void){ cmp_do(cpu_get_a(), rd(cpu_addr_inx())); }
void CMP_iny(void){
    eff_t e = cpu_addr_iny(); cmp_do(cpu_get_a(), rd(e.addr)); if(e.crossed) ADD_CYC(1);
}

void CPX_imm(void){ cmp_do(cpu_get_x(), fetch8()); }
void CPX_zp (void){ cmp_do(cpu_get_x(), rd(cpu_addr_zp())); }
void CPX_abs(void){ cmp_do(cpu_get_x(), rd(cpu_addr_abs())); }

void CPY_imm(void){ cmp_do(cpu_get_y(), fetch8()); }
void CPY_zp (void){ cmp_do(cpu_get_y(), rd(cpu_addr_zp())); }
void CPY_abs(void){ cmp_do(cpu_get_y(), rd(cpu_addr_abs())); }

// -----------------------------------------------------------------------------
// BIT
// -----------------------------------------------------------------------------
void BIT_zp (void){
    uint8_t m = rd(cpu_addr_zp());
    set_flag(FLAG_Z, (uint8_t)(cpu_get_a() & m) == 0);
    set_flag(FLAG_N, (m & 0x80)!=0);
    set_flag(FLAG_V, (m & 0x40)!=0);
}
void BIT_abs(void){
    uint8_t m = rd(cpu_addr_abs());
    set_flag(FLAG_Z, (uint8_t)(cpu_get_a() & m) == 0);
    set_flag(FLAG_N, (m & 0x80)!=0);
    set_flag(FLAG_V, (m & 0x40)!=0);
}

// -----------------------------------------------------------------------------
// SHIFTS / ROTATES (A and memory). Base cycles handled by table.
// -----------------------------------------------------------------------------
void ASL_A(void){ cpu_set_a(asl_val(cpu_get_a())); }
void LSR_A(void){ cpu_set_a(lsr_val(cpu_get_a())); }
void ROL_A(void){ cpu_set_a(rol_val(cpu_get_a())); }
void ROR_A(void){ cpu_set_a(ror_val(cpu_get_a())); }

void ASL_zp (void){ rmw_zp (cpu_fetch8(),               asl_val); }
void ASL_zpx(void){ rmw_zpx(cpu_fetch8(),               asl_val); }
void ASL_abs(void){ rmw_abs(cpu_fetch16(),              asl_val); }
void ASL_abx(void){ rmw_abx(cpu_fetch16(),              asl_val); }

void LSR_zp (void){ rmw_zp (cpu_fetch8(),               lsr_val); }
void LSR_zpx(void){ rmw_zpx(cpu_fetch8(),               lsr_val); }
void LSR_abs(void){ rmw_abs(cpu_fetch16(),              lsr_val); }
void LSR_abx(void){ rmw_abx(cpu_fetch16(),              lsr_val); }

void ROL_zp (void){ rmw_zp (cpu_fetch8(),               rol_val); }
void ROL_zpx(void){ rmw_zpx(cpu_fetch8(),               rol_val); }
void ROL_abs(void){ rmw_abs(cpu_fetch16(),              rol_val); }
void ROL_abx(void){ rmw_abx(cpu_fetch16(),              rol_val); }

void ROR_zp (void){ rmw_zp (cpu_fetch8(),               ror_val); }
void ROR_zpx(void){ rmw_zpx(cpu_fetch8(),               ror_val); }
void ROR_abs(void){ rmw_abs(cpu_fetch16(),              ror_val); }
void ROR_abx(void){ rmw_abx(cpu_fetch16(),              ror_val); }

// -----------------------------------------------------------------------------
// INC / DEC (memory). INX/DEX/INY/DEY (register).
// -----------------------------------------------------------------------------
static inline uint8_t inc_val(uint8_t v){ v=(uint8_t)(v+1); set_zn(v); return v; }
static inline uint8_t dec_val(uint8_t v){ v=(uint8_t)(v-1); set_zn(v); return v; }

void INC_zp (void){ rmw_zp (cpu_fetch8(),  inc_val); }
void INC_zpx(void){ rmw_zpx(cpu_fetch8(),  inc_val); }
void INC_abs(void){ rmw_abs(cpu_fetch16(), inc_val); }
void INC_abx(void){ rmw_abx(cpu_fetch16(), inc_val); }

void DEC_zp (void){ rmw_zp (cpu_fetch8(),  dec_val); }
void DEC_zpx(void){ rmw_zpx(cpu_fetch8(),  dec_val); }
void DEC_abs(void){ rmw_abs(cpu_fetch16(), dec_val); }
void DEC_abx(void){ rmw_abx(cpu_fetch16(), dec_val); }

void INX(void){ cpu_set_x((uint8_t)(cpu_get_x()+1)); set_zn(cpu_get_x()); }
void DEX(void){ cpu_set_x((uint8_t)(cpu_get_x()-1)); set_zn(cpu_get_x()); }
void INY(void){ cpu_set_y((uint8_t)(cpu_get_y()+1)); set_zn(cpu_get_y()); }
void DEY(void){ cpu_set_y((uint8_t)(cpu_get_y()-1)); set_zn(cpu_get_y()); }

// -----------------------------------------------------------------------------
// STACK / TRANSFERS
// -----------------------------------------------------------------------------
void PHA(void){ push8(cpu_get_a()); }
void PHP(void){
    uint8_t p = cpu_get_p();
    p |= (FLAG_B | FLAG_U);    // push copy with B and U set
    push8(p);
}
void PLA(void){ cpu_set_a(pop8()); set_zn(cpu_get_a()); }
void PLP(void){
    uint8_t p = pop8();
    p |= FLAG_U;               // internal U bit stays set
    cpu_set_p(p);
}

void TAX(void){ cpu_set_x(cpu_get_a()); set_zn(cpu_get_x()); }
void TXA(void){ cpu_set_a(cpu_get_x()); set_zn(cpu_get_a()); }
void TAY(void){ cpu_set_y(cpu_get_a()); set_zn(cpu_get_y()); }
void TYA(void){ cpu_set_a(cpu_get_y()); set_zn(cpu_get_a()); }
void TSX(void){ cpu_set_x(cpu_get_sp()); set_zn(cpu_get_x()); }
void TXS(void){ cpu_set_sp(cpu_get_x()); }

// -----------------------------------------------------------------------------
// FLAGS / NOP
// -----------------------------------------------------------------------------
void CLC(void){ set_flag(FLAG_C, 0); }
void SEC(void){ set_flag(FLAG_C, 1); }
void CLI(void){ set_flag(FLAG_I, 0); }
void SEI(void){ set_flag(FLAG_I, 1); }
void CLD(void){ set_flag(FLAG_D, 0); }
void SED(void){ set_flag(FLAG_D, 1); }
void CLV(void){ set_flag(FLAG_V, 0); }
void NOP(void){ /* nothing */ }

// -----------------------------------------------------------------------------
// ILLEGAL / UNIMPLEMENTED — treat as NOP
// -----------------------------------------------------------------------------
void op_illegal(void){ /* NOP for now */ }
