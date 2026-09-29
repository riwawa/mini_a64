#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <stdint.h>
#include <stddef.h>


typedef enum
{
    /* ADD */
    OP_ADD_IMM,
    OP_ADD_SHIFT,
    OP_ADD_EXT,

    /* ADDS */
    OP_ADDS_IMM,
    OP_ADDS_SHIFT,
    OP_ADDS_EXT,

    /* SUB */
    OP_SUB_IMM,
    OP_SUB_SHIFT,
    OP_SUB_EXT,

    /* SUBS */
    OP_SUBS_IMM,
    OP_SUBS_SHIFT,
    OP_SUBS_EXT,

    /* Logical */
    OP_AND_IMM,
    OP_AND_SHIFT,

    OP_ANDS_IMM,
    OP_ANDS_SHIFT,

    OP_ORR_IMM,
    OP_ORR_SHIFT,

    OP_EOR_IMM,
    OP_EOR_SHIFT,

    /* Move wide */
    OP_MOVN,
    OP_MOVZ,
    OP_MOVK,

    /* Memory */
    OP_LDR,
    OP_STR,

    /* Branch immediate */
    OP_B,
    OP_BL,

    /* Branch condicional */
    OP_B_COND,

    /* Branch register */
    OP_BR,
    OP_BLR,
    OP_RET,

    /* Inválida */
    OP_UNKNOWN

} Opcode;


typedef struct
{
    Opcode opcode;

    /*
     * Registradores
     */
    uint32_t rd;
    uint32_t rn;
    uint32_t rm;

    /*
     * Immediate genérico
     */
    uint64_t immediate;

    /*
     * Shifted register
     */
    uint32_t shift_type;
    uint32_t shift_amount;

    /*
     * Extended register
     */
    uint32_t extend_type;

    /*
     * B.cond
     */
    uint32_t condition;

    const char *label_start;
    size_t label_length;

} Instruction;


#endif