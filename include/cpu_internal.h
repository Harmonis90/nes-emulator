//
// Created by Seth on 9/17/2025.
//

#ifndef NES_CPU_INTERNAL_H
#define NES_CPU_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

// ----------------------------
// Instruction byte fetch (advances PC)
// ----------------------------
uint8_t fetch8(void); // read byte at PC, then PC++
uint16_t fetch16(void); // little-endian 16-bit at PC, then PC += 2

// ----------------------------
// Stack operations (use CPU stack at $0100 + SP)
// ----------------------------
void push8(uint8_t v);
void push16(uint16_t v);
uint8_t pop8(void);
uint16_t pop16(void);

// ----------------------------
// Flag helpers
// ----------------------------
void set_flag(uint8_t flag_mask, int on); // on=0 clears, nonzero sets
uint8_t get_flag(uint8_t flag_mask); // returns 0 or 1
void set_zn(uint8_t v);  // sets Z and N from value

// ----------------------------
// Interrupt enter helper (used by BRK/IRQ/NMI)
//   vec       : vector address (e.g., VEC_IRQ_BRK, VEC_NMI)
//   set_break : nonzero sets B on pushed P; zero clears B on pushed P
// ----------------------------
void interrupt_enter(uint16_t vec, int set_break);

#ifdef __cplusplus
}
#endif


#endif //NES_CPU_INTERNAL_H
