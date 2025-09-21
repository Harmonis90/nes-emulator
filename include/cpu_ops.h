// include/cpu_ops.h
// 6502 opcode handler prototypes for the NES CPU core.
// Each handler executes the full instruction (fetches operands as needed,
// performs memory I/O via bus.h, updates flags/state, and accounts cycles).

#ifndef NES_CPU_OPS_H
#define NES_CPU_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// System / flow control
// -----------------------------------------------------------------------------
void BRK(void);
void RTI(void);
void RTS(void);
void JSR_abs(void);
void JMP_abs(void);
void JMP_ind(void);

// -----------------------------------------------------------------------------
// Branches
// -----------------------------------------------------------------------------
void BPL(void);
void BMI(void);
void BVC(void);
void BVS(void);
void BCC(void);
void BCS(void);
void BNE(void);
void BEQ(void);

// -----------------------------------------------------------------------------
// Loads
// -----------------------------------------------------------------------------
void LDA_imm(void);
void LDA_zp(void);
void LDA_zpx(void);
void LDA_abs(void);
void LDA_abx(void);
void LDA_aby(void);
void LDA_inx(void);
void LDA_iny(void);

void LDX_imm(void);
void LDX_zp(void);
void LDX_zpy(void);
void LDX_abs(void);
void LDX_aby(void);

void LDY_imm(void);
void LDY_zp(void);
void LDY_zpx(void);
void LDY_abs(void);
void LDY_abx(void);

// -----------------------------------------------------------------------------
// Stores
// -----------------------------------------------------------------------------
void STA_zp(void);
void STA_zpx(void);
void STA_abs(void);
void STA_abx(void);
void STA_aby(void);
void STA_inx(void);
void STA_iny(void);

void STX_zp(void);
void STX_zpy(void);
void STX_abs(void);

void STY_zp(void);
void STY_zpx(void);
void STY_abs(void);

// -----------------------------------------------------------------------------
// ALU: ADC / SBC
// -----------------------------------------------------------------------------
void ADC_imm(void);
void ADC_zp(void);
void ADC_zpx(void);
void ADC_abs(void);
void ADC_abx(void);
void ADC_aby(void);
void ADC_inx(void);
void ADC_iny(void);

void SBC_imm(void);
void SBC_zp(void);
void SBC_zpx(void);
void SBC_abs(void);
void SBC_abx(void);
void SBC_aby(void);
void SBC_inx(void);
void SBC_iny(void);

// -----------------------------------------------------------------------------
// ALU: AND / ORA / EOR
// -----------------------------------------------------------------------------
void AND_imm(void);
void AND_zp(void);
void AND_zpx(void);
void AND_abs(void);
void AND_abx(void);
void AND_aby(void);
void AND_inx(void);
void AND_iny(void);

void ORA_imm(void);
void ORA_zp(void);
void ORA_zpx(void);
void ORA_abs(void);
void ORA_abx(void);
void ORA_aby(void);
void ORA_inx(void);
void ORA_iny(void);

void EOR_imm(void);
void EOR_zp(void);
void EOR_zpx(void);
void EOR_abs(void);
void EOR_abx(void);
void EOR_aby(void);
void EOR_inx(void);
void EOR_iny(void);

// -----------------------------------------------------------------------------
// Compares
// -----------------------------------------------------------------------------
void CMP_imm(void);
void CMP_zp(void);
void CMP_zpx(void);
void CMP_abs(void);
void CMP_abx(void);
void CMP_aby(void);
void CMP_inx(void);
void CMP_iny(void);

void CPX_imm(void);
void CPX_zp(void);
void CPX_abs(void);

void CPY_imm(void);
void CPY_zp(void);
void CPY_abs(void);

// -----------------------------------------------------------------------------
// BIT test
// -----------------------------------------------------------------------------
void BIT_zp(void);
void BIT_abs(void);

// -----------------------------------------------------------------------------
// Shifts / Rotates (Accumulator and Memory)
// -----------------------------------------------------------------------------
void ASL_A(void);
void ASL_zp(void);
void ASL_zpx(void);
void ASL_abs(void);
void ASL_abx(void);

void LSR_A(void);
void LSR_zp(void);
void LSR_zpx(void);
void LSR_abs(void);
void LSR_abx(void);

void ROL_A(void);
void ROL_zp(void);
void ROL_zpx(void);
void ROL_abs(void);
void ROL_abx(void);

void ROR_A(void);
void ROR_zp(void);
void ROR_zpx(void);
void ROR_abs(void);
void ROR_abx(void);

// -----------------------------------------------------------------------------
// INC / DEC (memory) and register INC/DEC
// -----------------------------------------------------------------------------
void INC_zp(void);
void INC_zpx(void);
void INC_abs(void);
void INC_abx(void);

void DEC_zp(void);
void DEC_zpx(void);
void DEC_abs(void);
void DEC_abx(void);

void INX(void);
void DEX(void);
void INY(void);
void DEY(void);

// -----------------------------------------------------------------------------
// Stack and register transfers
// -----------------------------------------------------------------------------
void PHA(void);
void PHP(void);
void PLA(void);
void PLP(void);

void TAX(void);
void TXA(void);
void TAY(void);
void TYA(void);
void TSX(void);
void TXS(void);

// -----------------------------------------------------------------------------
// Flags and NOP
// -----------------------------------------------------------------------------
void CLC(void);
void SEC(void);
void CLI(void);
void SEI(void);
void CLD(void);
void SED(void);
void CLV(void);
void NOP(void);

// -----------------------------------------------------------------------------
// Illegal / unimplemented opcode handler
// -----------------------------------------------------------------------------
void op_illegal(void);

#ifdef __cplusplus
}
#endif

#endif // NES_CPU_OPS_H
