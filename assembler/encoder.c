#include "encoder.h"

#include <stddef.h>
#include <stdint.h>


static int valid_reg(uint32_t reg)
{
    return reg <= 30;
}

static int build_logical_mask(
    uint32_t N,
    uint32_t immr,
    uint32_t imms,
    uint64_t *mask_out)
{
    uint32_t value =
        (N << 6) |
        ((~imms) & 0x3F);


    int len = -1;

    for (int i = 6; i >= 0; i--)
    {
        if (value & (1u << i))
        {
            len = i;
            break;
        }
    }


    if (len < 1)
        return 0;


    uint32_t levels =
        (1u << len) - 1;


    uint32_t S =
        imms & levels;

    uint32_t R =
        immr & levels;


    if (S == levels)
        return 0;


    uint32_t esize =
        1u << len;


    uint64_t element =
        (1ULL << (S + 1)) - 1;


    uint64_t element_mask;

    if (esize == 64)
        element_mask = UINT64_MAX;
    else
        element_mask =
            (1ULL << esize) - 1;


    R %= esize;


    if (R != 0)
    {
        element =
            ((element >> R) |
             (element << (esize - R)))
            & element_mask;
    }


    uint64_t result = 0;


    for (
        uint32_t pos = 0;
        pos < 64;
        pos += esize)
    {
        result |=
            element << pos;
    }


    *mask_out = result;

    return 1;
}

static int encode_logical_immediate_fields(
    uint64_t immediate,
    uint32_t *N_out,
    uint32_t *immr_out,
    uint32_t *imms_out)
{
    for (uint32_t N = 0;
         N <= 1;
         N++)
    {
        for (uint32_t immr = 0;
             immr < 64;
             immr++)
        {
            for (uint32_t imms = 0;
                 imms < 64;
                 imms++)
            {
                uint64_t mask;


                if (!build_logical_mask(
                        N,
                        immr,
                        imms,
                        &mask))
                {
                    continue;
                }


                if (mask == immediate)
                {
                    *N_out = N;
                    *immr_out = immr;
                    *imms_out = imms;

                    return 1;
                }
            }
        }
    }

    return 0;
}

static int encode_add_sub_imm(
    const Instruction *instr,
    uint32_t *out)
{
    uint32_t op;
    uint32_t S;


    switch (instr->opcode)
    {
        case OP_ADD_IMM:
            op = 0;
            S = 0;
            break;

        case OP_ADDS_IMM:
            op = 0;
            S = 1;
            break;

        case OP_SUB_IMM:
            op = 1;
            S = 0;
            break;

        case OP_SUBS_IMM:
            op = 1;
            S = 1;
            break;

        default:
            return 0;
    }


    if (!valid_reg(instr->rd) ||
        !valid_reg(instr->rn))
    {
        return 0;
    }

    if (instr->immediate > 0xFFF)
        return 0;


    uint32_t word =
        0x11000000;


    word |= 1u << 31;


    word |=
        (op & 1u) << 30;

    word |=
        (S & 1u) << 29;


    word |=
        ((uint32_t)instr->immediate & 0xFFF)
        << 10;

    word |=
        (instr->rn & 0x1F)
        << 5;

    word |=
        instr->rd & 0x1F;


    *out = word;

    return 1;
}

static int encode_add_sub_shift(
    const Instruction *instr,
    uint32_t *out)
{
    uint32_t op;
    uint32_t S;


    switch (instr->opcode)
    {
        case OP_ADD_SHIFT:
            op = 0;
            S = 0;
            break;

        case OP_ADDS_SHIFT:
            op = 0;
            S = 1;
            break;

        case OP_SUB_SHIFT:
            op = 1;
            S = 0;
            break;

        case OP_SUBS_SHIFT:
            op = 1;
            S = 1;
            break;

        default:
            return 0;
    }


    if (!valid_reg(instr->rd) ||
        !valid_reg(instr->rn) ||
        !valid_reg(instr->rm))
    {
        return 0;
    }


    if (instr->shift_type > 2)
        return 0;


    if (instr->shift_amount > 63)
        return 0;


    uint32_t word =
        0x0B000000;


    word |=
        1u << 31;


    word |=
        (op & 1u) << 30;

    word |=
        (S & 1u) << 29;


    /*
     * shift [23:22]
     */
    word |=
        (instr->shift_type & 0x3)
        << 22;


    /*
     * Rm [20:16]
     */
    word |=
        (instr->rm & 0x1F)
        << 16;


    /*
     * imm6 [15:10]
     */
    word |=
        (instr->shift_amount & 0x3F)
        << 10;


    /*
     * Rn [9:5]
     */
    word |=
        (instr->rn & 0x1F)
        << 5;


    /*
     * Rd [4:0]
     */
    word |=
        instr->rd & 0x1F;


    *out = word;

    return 1;
}


static int encode_add_sub_ext(
    const Instruction *instr,
    uint32_t *out)
{
    uint32_t op;
    uint32_t S;


    switch (instr->opcode)
    {
        case OP_ADD_EXT:
            op = 0;
            S = 0;
            break;

        case OP_ADDS_EXT:
            op = 0;
            S = 1;
            break;

        case OP_SUB_EXT:
            op = 1;
            S = 0;
            break;

        case OP_SUBS_EXT:
            op = 1;
            S = 1;
            break;

        default:
            return 0;
    }


    if (!valid_reg(instr->rd) ||
        !valid_reg(instr->rn) ||
        !valid_reg(instr->rm))
    {
        return 0;
    }


    if (instr->extend_type > 7)
        return 0;


    if (instr->shift_amount > 4)
        return 0;


    uint32_t word =
        0x0B200000;


    word |=
        1u << 31;


    word |=
        (op & 1u) << 30;

    word |=
        (S & 1u) << 29;


    /*
     * Rm
     */
    word |=
        (instr->rm & 0x1F)
        << 16;


    /*
     * option
     */
    word |=
        (instr->extend_type & 0x7)
        << 13;


    /*
     * imm3
     */
    word |=
        (instr->shift_amount & 0x7)
        << 10;


    /*
     * Rn
     */
    word |=
        (instr->rn & 0x1F)
        << 5;


    /*
     * Rd
     */
    word |=
        instr->rd & 0x1F;


    *out = word;

    return 1;
}

static int encode_logical_shift(
    const Instruction *instr,
    uint32_t *out)
{
    uint32_t opc;


    switch (instr->opcode)
    {
        case OP_AND_SHIFT:
            opc = 0;
            break;

        case OP_ORR_SHIFT:
            opc = 1;
            break;

        case OP_EOR_SHIFT:
            opc = 2;
            break;

        case OP_ANDS_SHIFT:
            opc = 3;
            break;

        default:
            return 0;
    }


    if (!valid_reg(instr->rd) ||
        !valid_reg(instr->rn) ||
        !valid_reg(instr->rm))
    {
        return 0;
    }


    /*
     * Logical aceita:
     *
     * 0 LSL
     * 1 LSR
     * 2 ASR
     * 3 ROR
     */
    if (instr->shift_type > 3)
        return 0;


    if (instr->shift_amount > 63)
        return 0;


    uint32_t word =
        0x0A000000;


    /*
     * sf
     */
    word |=
        1u << 31;


    /*
     * opc [30:29]
     */
    word |=
        (opc & 0x3)
        << 29;


    /*
     * shift [23:22]
     */
    word |=
        (instr->shift_type & 0x3)
        << 22;


    /*
     * Rm
     */
    word |=
        (instr->rm & 0x1F)
        << 16;


    /*
     * imm6
     */
    word |=
        (instr->shift_amount & 0x3F)
        << 10;


    /*
     * Rn
     */
    word |=
        (instr->rn & 0x1F)
        << 5;


    /*
     * Rd
     */
    word |=
        instr->rd & 0x1F;


    *out = word;

    return 1;
}

static int encode_logical_imm(
    const Instruction *instr,
    uint32_t *out)
{
    uint32_t opc;


    switch (instr->opcode)
    {
        case OP_AND_IMM:
            opc = 0;
            break;

        case OP_ORR_IMM:
            opc = 1;
            break;

        case OP_EOR_IMM:
            opc = 2;
            break;

        case OP_ANDS_IMM:
            opc = 3;
            break;

        default:
            return 0;
    }


    if (!valid_reg(instr->rd) ||
        !valid_reg(instr->rn))
    {
        return 0;
    }


    uint32_t N;
    uint32_t immr;
    uint32_t imms;


    if (!encode_logical_immediate_fields(
            instr->immediate,
            &N,
            &immr,
            &imms))
    {
        return 0;
    }


    uint32_t word =
        0x12000000;


    /*
     * sf = 1
     */
    word |=
        1u << 31;


    /*
     * opc
     */
    word |=
        (opc & 0x3)
        << 29;


    /*
     * N
     */
    word |=
        (N & 1u)
        << 22;


    /*
     * immr
     */
    word |=
        (immr & 0x3F)
        << 16;


    /*
     * imms
     */
    word |=
        (imms & 0x3F)
        << 10;


    /*
     * Rn
     */
    word |=
        (instr->rn & 0x1F)
        << 5;


    /*
     * Rd
     */
    word |=
        instr->rd & 0x1F;


    *out = word;

    return 1;
}

static int encode_move_wide(
    const Instruction *instr,
    uint32_t *out)
{
    uint32_t opc;


    switch (instr->opcode)
    {
        case OP_MOVN:
            opc = 0;
            break;

        case OP_MOVZ:
            opc = 2;
            break;

        case OP_MOVK:
            opc = 3;
            break;

        default:
            return 0;
    }


    if (!valid_reg(instr->rd))
        return 0;


    if (instr->immediate > 0xFFFF)
        return 0;


    /*
     * hw representa:
     *
     * 0 -> LSL #0
     * 1 -> LSL #16
     * 2 -> LSL #32
     * 3 -> LSL #48
     */
    if (
        instr->shift_amount != 0 &&
        instr->shift_amount != 16 &&
        instr->shift_amount != 32 &&
        instr->shift_amount != 48)
    {
        return 0;
    }


    uint32_t hw =
        instr->shift_amount / 16;


    uint32_t word =
        0x12800000;


    /*
     * sf = 1
     */
    word |=
        1u << 31;


    /*
     * opc
     */
    word |=
        (opc & 0x3)
        << 29;


    /*
     * hw
     */
    word |=
        (hw & 0x3)
        << 21;


    /*
     * imm16
     */
    word |=
        ((uint32_t)instr->immediate & 0xFFFF)
        << 5;


    /*
     * Rd
     */
    word |=
        instr->rd & 0x1F;


    *out = word;

    return 1;
}


static int encode_load_store(
    const Instruction *instr,
    uint32_t *out)
{
    if (
        instr->opcode != OP_LDR &&
        instr->opcode != OP_STR)
    {
        return 0;
    }


    if (!valid_reg(instr->rd) ||
        !valid_reg(instr->rn))
    {
        return 0;
    }

    if ((instr->immediate % 8) != 0)
        return 0;


    uint64_t scaled =
        instr->immediate / 8;


    if (scaled > 0xFFF)
        return 0;

    uint32_t word;


    if (instr->opcode == OP_STR)
        word = 0xF9000000;
    else
        word = 0xF9400000;


    word |=
        ((uint32_t)scaled & 0xFFF)
        << 10;


    /*
     * Rn
     */
    word |=
        (instr->rn & 0x1F)
        << 5;


    /*
     * Rt
     *
     * O parser atualmente guarda Rt em rd.
     */
    word |=
        instr->rd & 0x1F;


    *out = word;

    return 1;
}

static int resolve_label(
    const Instruction *instr,
    const SymbolTable *symbols,
    uint64_t *target)
{
    if (symbols == NULL)
        return 0;


    if (instr->label_start == NULL ||
        instr->label_length == 0)
    {
        return 0;
    }


    return symbol_table_find(
        symbols,
        instr->label_start,
        instr->label_length,
        target
    );
}

static int encode_branch_imm(
    const Instruction *instr,
    uint64_t pc,
    const SymbolTable *symbols,
    uint32_t *out)
{
    uint64_t target;


    if (!resolve_label(
            instr,
            symbols,
            &target))
    {
        return 0;
    }
    int64_t byte_offset =
        (int64_t)target -
        (int64_t)pc;

    if ((byte_offset % 4) != 0)
        return 0;


    int64_t offset =
        byte_offset / 4;

    if (
        offset < -(1LL << 25) ||
        offset >  ((1LL << 25) - 1))
    {
        return 0;
    }


    uint32_t word;


    if (instr->opcode == OP_B)
    {
        word = 0x14000000;
    }
    else if (instr->opcode == OP_BL)
    {
        word = 0x94000000;
    }
    else
    {
        return 0;
    }

    uint32_t imm26 =
        (uint32_t)offset &
        0x03FFFFFF;


    word |= imm26;


    *out = word;

    return 1;
}

static int encode_branch_cond(
    const Instruction *instr,
    uint64_t pc,
    const SymbolTable *symbols,
    uint32_t *out)
{
    if (instr->condition > 0xF)
        return 0;


    uint64_t target;


    if (!resolve_label(
            instr,
            symbols,
            &target))
    {
        return 0;
    }


    int64_t byte_offset =
        (int64_t)target -
        (int64_t)pc;


    if ((byte_offset % 4) != 0)
        return 0;


    int64_t offset =
        byte_offset / 4;


    /*
     * signed imm19
     *
     * -2^18 ... 2^18 - 1
     */
    if (
        offset < -(1LL << 18) ||
        offset >  ((1LL << 18) - 1))
    {
        return 0;
    }


    uint32_t imm19 =
        (uint32_t)offset &
        0x7FFFF;


    uint32_t word =
        0x54000000;


    /*
     * imm19 [23:5]
     */
    word |=
        imm19 << 5;


    /*
     * cond [3:0]
     */
    word |=
        instr->condition & 0xF;


    *out = word;

    return 1;
}


static int encode_branch_register(
    const Instruction *instr,
    uint32_t *out)
{
    if (!valid_reg(instr->rn))
        return 0;


    uint32_t word;


    switch (instr->opcode)
    {
        case OP_BR:
            word = 0xD61F0000;
            break;

        case OP_BLR:
            word = 0xD63F0000;
            break;

        case OP_RET:
            word = 0xD65F0000;
            break;

        default:
            return 0;
    }


    /*
     * Rn [9:5]
     */
    word |=
        (instr->rn & 0x1F)
        << 5;


    *out = word;

    return 1;
}



int encode_instruction(
    const Instruction *instr,
    uint64_t pc,
    const SymbolTable *symbols,
    uint32_t *out)
{
    if (instr == NULL ||
        out == NULL)
    {
        return 0;
    }


    switch (instr->opcode)
    {
        /* =================================================
         * ADD / SUB IMMEDIATE
         * =================================================
         */

        case OP_ADD_IMM:
        case OP_ADDS_IMM:
        case OP_SUB_IMM:
        case OP_SUBS_IMM:

            return encode_add_sub_imm(
                instr,
                out
            );


        /* =================================================
         * ADD / SUB SHIFTED
         * =================================================
         */

        case OP_ADD_SHIFT:
        case OP_ADDS_SHIFT:
        case OP_SUB_SHIFT:
        case OP_SUBS_SHIFT:

            return encode_add_sub_shift(
                instr,
                out
            );


        /* =================================================
         * ADD / SUB EXTENDED
         * =================================================
         */

        case OP_ADD_EXT:
        case OP_ADDS_EXT:
        case OP_SUB_EXT:
        case OP_SUBS_EXT:

            return encode_add_sub_ext(
                instr,
                out
            );


        /* =================================================
         * LOGICAL IMMEDIATE
         * =================================================
         */

        case OP_AND_IMM:
        case OP_ANDS_IMM:
        case OP_ORR_IMM:
        case OP_EOR_IMM:

            return encode_logical_imm(
                instr,
                out
            );


        /* =================================================
         * LOGICAL SHIFTED
         * =================================================
         */

        case OP_AND_SHIFT:
        case OP_ANDS_SHIFT:
        case OP_ORR_SHIFT:
        case OP_EOR_SHIFT:

            return encode_logical_shift(
                instr,
                out
            );


        /* =================================================
         * MOV WIDE
         * =================================================
         */

        case OP_MOVN:
        case OP_MOVZ:
        case OP_MOVK:

            return encode_move_wide(
                instr,
                out
            );


        /* =================================================
         * LOAD / STORE
         * =================================================
         */

        case OP_LDR:
        case OP_STR:

            return encode_load_store(
                instr,
                out
            );


        /* =================================================
         * B / BL
         * =================================================
         */

        case OP_B:
        case OP_BL:

            return encode_branch_imm(
                instr,
                pc,
                symbols,
                out
            );


        /* =================================================
         * B.COND
         * =================================================
         */

        case OP_B_COND:

            return encode_branch_cond(
                instr,
                pc,
                symbols,
                out
            );


        /* =================================================
         * BR / BLR / RET
         * =================================================
         */

        case OP_BR:
        case OP_BLR:
        case OP_RET:

            return encode_branch_register(
                instr,
                out
            );


        case OP_UNKNOWN:
        default:
            return 0;
    }
}