#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

#include "instruction.h"
#include "symbol_table.h"

int encode_instruction(
    const Instruction *instr,
    uint64_t pc,
    const SymbolTable *symbols,
    uint32_t *out
);


#endif