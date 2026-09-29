
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

#define MEMORY_SIZE (1 << 20)   /* 1 MiB */
uint8_t memory[MEMORY_SIZE];

uint64_t reg[REG_COUNT];
uint64_t pc;

enum
{
    FLAG_V = 1 << 0,
    FLAG_C = 1 << 1,
    FLAG_Z = 1 << 2,
    FLAG_N = 1 << 3
};
uint8_t nzcv;

enum opcode
{
    OP_ADD_IMM,
    OP_ADD_SHIFT,
    OP_ADD_EXT,

    OP_ADDS_IMM,
    OP_ADDS_SHIFT,
    OP_ADDS_EXT,

    OP_SUB_IMM,
    OP_SUB_SHIFT,
    OP_SUB_EXT,

    OP_SUBS_IMM,
    OP_SUBS_SHIFT,
    OP_SUBS_EXT,

    OP_AND_IMM,
    OP_AND_SHIFT,

    OP_ANDS_IMM,
    OP_ANDS_SHIFT,

    OP_ORR_IMM,
    OP_ORR_SHIFT,

    OP_EOR_IMM,
    OP_EOR_SHIFT,

    OP_MOVN,
    OP_MOVZ,
    OP_MOVK,

    OP_LDR,
    OP_STR,

    OP_B,
    OP_BL,

    OP_B_COND,

    OP_BR,
    OP_BLR,
    OP_RET,

    OP_UNKNOWN
};

uint64_t sign_extend(uint64_t x, unsigned bit_count)
{
    uint64_t sign_bit = 1ULL << (bit_count - 1);

    if (x & sign_bit)
    {
        uint64_t mask = ~((1ULL << bit_count) - 1);
        x |= mask;
    }

    return x;
}


void update_nz(uint64_t result)
{
    nzcv &= ~(FLAG_N | FLAG_Z);

    if (result == 0)
    {
        nzcv |= FLAG_Z;
    }

    if (result >> 63)
    {
        nzcv |= FLAG_N;
    }
}


uint32_t mem_read32(uint64_t address)
{
    if (address + 3 >= MEMORY_SIZE)
    {
        fprintf(stderr, "memory read out of bounds\n");
        exit(EXIT_FAILURE);
    }

    uint32_t value = 0;

    value |= (uint32_t)memory[address];
    value |= (uint32_t)memory[address + 1] << 8;
    value |= (uint32_t)memory[address + 2] << 16;
    value |= (uint32_t)memory[address + 3] << 24;

    return value;
}


uint64_t mem_read64(uint64_t address)
{
    if (address + 7 >= MEMORY_SIZE)
    {
        fprintf(stderr, "memory read out of bounds\n");
        exit(EXIT_FAILURE);
    }

    uint64_t value = 0;

    for (int i = 0; i < 8; i++)
    {
        value |= (uint64_t)memory[address + i] << (i * 8);
    }

    return value;
}


void mem_write64(uint64_t address, uint64_t value)
{
    if (address + 7 >= MEMORY_SIZE)
    {
        fprintf(stderr, "memory write out of bounds\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 8; i++)
    {
        memory[address + i] =
            (uint8_t)((value >> (i * 8)) & 0xFF);
    }
}


size_t load_image(
    const char *path,
    uint64_t start_address)
{
    FILE *file =
        fopen(path, "rb");

    if (file == NULL)
    {
        perror("could not open image");
        return 0;
    }

    if (start_address >= MEMORY_SIZE)
    {
        fprintf(
            stderr,
            "program start address out of memory\n"
        );
        fclose(file);
        return 0;
    }

    size_t max_size =
        MEMORY_SIZE -
        (size_t)start_address;


    size_t bytes_read =
        fread(
            &memory[start_address],
            1,
            max_size,
            file
        );

    if (ferror(file))
    {
        perror("could not read image");
        fclose(file);
        return 0;
    }


    fclose(file);

    printf(
        "loaded %zu bytes at 0x%llx\n",
        bytes_read,
        (unsigned long long)
            start_address
    );
    return bytes_read;
}

uint32_t extract_bits(
    uint32_t instr,
    unsigned start,
    unsigned width)
{
    uint32_t mask = (1u << width) - 1;

    return (instr >> start) & mask;
}


enum opcode decode_instruction(uint32_t instr)
{
	// ADD AND SUB
		// immediate
    const uint32_t MASK_ADD_SUB_IMM    = 0x1F800000;
    const uint32_t PATTERN_ADD_SUB_IMM = 0x11000000;
		// shifted
    const uint32_t MASK_ADD_SUB_SHIFTED    = 0x1F200000;
    const uint32_t PATTERN_ADD_SUB_SHIFTED = 0x0B000000;
		// extended
    const uint32_t MASK_ADD_SUB_EXTENDED	 = 0x1FE00000;
    const uint32_t PATTERN_ADD_SUB_EXTENDED = 0x0B200000;	

	// LOGICAL REGISTERS
		// shifted
    const uint32_t MASK_LOGICAL_SHIFT   = 0x1F200000;
    const uint32_t PATTERN_LOGICAL_SHIFT = 0x0A000000;
		// immediate
    const uint32_t MASK_LOGICAL_IMM   = 0x1F800000;
    const uint32_t PATTERN_LOGICAL_IMM = 0x12000000;

	// MOVE
	const uint32_t MASK_MOVE_WIDE = 0x1F800000;
	const uint32_t PATTERN_MOVE_WIDE = 0x25 << 23;

	// CONDITIONAL
	const uint32_t MASK_UNCOND_BRANCH = 0x7C000000;
	const uint32_t PATTERN_UNCOND_BRANCH = 0x14000000;
	const uint32_t MASK_COND_BRANCH = 0xFF000010;
	const uint32_t PATTERN_COND_BRANCH = 0x54000000;

	// branch to register
	const uint32_t MASK_BRANCH_REG =0xFF9FFC1F;
	const uint32_t PATTERN_BRANCH_REG =0xD61F0000;

	// load and store immediate
	const uint32_t MASK_LOAD_STORE_UIMM = 0x3F000000;
	const uint32_t PATTERN_LOAD_STORE_UIMM = 0x39000000;

    if ((instr & MASK_ADD_SUB_IMM) == PATTERN_ADD_SUB_IMM)
    {
        uint32_t sf = (instr >> 31) & 0x1;
        uint32_t op = (instr >> 30) & 0x1;
        uint32_t S  = (instr >> 29) & 0x1;

        if (sf == 0)
            return OP_UNKNOWN;

        if (op == 0 && S == 0)
            return OP_ADD_IMM;

        if (op == 0 && S == 1)
            return OP_ADDS_IMM;

        if (op == 1 && S == 0)
            return OP_SUB_IMM;

        if (op == 1 && S == 1)
            return OP_SUBS_IMM;
    }
	if ((instr & MASK_ADD_SUB_SHIFTED) == PATTERN_ADD_SUB_SHIFTED){
        uint32_t sf = (instr >> 31) & 0x1;
        uint32_t op = (instr >> 30) & 0x1;
        uint32_t S  = (instr >> 29) & 0x1;

		if (sf == 0) return OP_UNKNOWN;

		if (op == 0 && S == 0)
            return OP_ADD_SHIFT;

        if (op == 0 && S == 1)
            return OP_ADDS_SHIFT;

        if (op == 1 && S == 0)
            return OP_SUB_SHIFT;

        if (op == 1 && S == 1)
            return OP_SUBS_SHIFT;
	}
	if ((instr & MASK_ADD_SUB_EXTENDED) == PATTERN_ADD_SUB_EXTENDED){
        uint32_t sf = (instr >> 31) & 0x1;
        uint32_t op = (instr >> 30) & 0x1;
        uint32_t S  = (instr >> 29) & 0x1;

		if (sf == 0) return OP_UNKNOWN;

		if (op == 0 && S == 0)
            return OP_ADD_EXT;

        if (op == 0 && S == 1)
            return OP_ADDS_EXT;

        if (op == 1 && S == 0)
            return OP_SUB_EXT;

        if (op == 1 && S == 1)
            return OP_SUBS_EXT;
	}
    if ((instr & MASK_LOGICAL_SHIFT) == PATTERN_LOGICAL_SHIFT)
    {
		uint32_t sf =
			(instr >> 31) & 0x1;

		uint32_t opc =
			(instr >> 29) & 0x3;
		if (sf == 0)
			return OP_UNKNOWN;

		if (opc == 0b00)
			return OP_AND_SHIFT;

		if (opc == 0b01)
			return OP_ORR_SHIFT;

		if (opc == 0b10)
			return OP_EOR_SHIFT;

		if (opc == 0b11)
			return OP_ANDS_SHIFT;
    }
	if ((instr & MASK_LOGICAL_IMM) == PATTERN_LOGICAL_IMM)
	{
		uint32_t sf =
			(instr >> 31) & 0x1;

		uint32_t opc =
			(instr >> 29) & 0x3;

		if (sf == 0)
			return OP_UNKNOWN;

		if (opc == 0b00)
			return OP_AND_IMM;

		if (opc == 0b01)
			return OP_ORR_IMM;

		if (opc == 0b10)
			return OP_EOR_IMM;

		if (opc == 0b11)
			return OP_ANDS_IMM;
	}
    if ((instr & MASK_MOVE_WIDE) == PATTERN_MOVE_WIDE)
	{
		uint32_t sf =
			(instr >> 31) & 0x1;

		uint32_t opc =
			(instr >> 29) & 0x3;

		if (sf == 0)
			return OP_UNKNOWN;

		if (opc == 0b00)
			return OP_MOVN;

		if (opc == 0b10)
			return OP_MOVZ;

		if (opc == 0b11)
			return OP_MOVK;
	}
	if ((instr & MASK_UNCOND_BRANCH) == PATTERN_UNCOND_BRANCH)
	{
		uint32_t op =
			(instr >> 31) & 0x1;

		if (op == 0)
			return OP_B;

		if (op == 1)
			return OP_BL;
	}
	if ((instr & MASK_BRANCH_REG) == PATTERN_BRANCH_REG)
	{
		uint32_t op =
			(instr >> 21) & 0x3;

		if (op == 0b00)
			return OP_BR;

		if (op == 0b01)
			return OP_BLR;

		if (op == 0b10)
			return OP_RET;

		return OP_UNKNOWN;
	}
	if ((instr & MASK_COND_BRANCH) == PATTERN_COND_BRANCH)
	{
		return OP_B_COND;
	}
	if ((instr & MASK_LOAD_STORE_UIMM)== PATTERN_LOAD_STORE_UIMM)
	{
		uint32_t size = (instr >> 30) & 0x3;
		uint32_t opc = (instr >> 22) & 0x3;
		if (size != 0b11)
        	return OP_UNKNOWN;

		if (opc == 0b00)
			return OP_STR;

		if (opc == 0b01)
			return OP_LDR;
	}
	
	return OP_UNKNOWN;

}

int build_logical_immediate_mask(
    uint32_t N,
    uint32_t immr,
    uint32_t imms,
    uint64_t *mask_out)
{
    uint32_t value =
        (N << 6) | ((~imms) & 0x3F);

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


    /*
     * Pega somente os bits relevantes
     * de imms e immr.
     */
    uint32_t S =
        imms & levels;

    uint32_t R =
        immr & levels;


    /*
     * Encoding reservado/inválido.
     */
    if (S == levels)
        return 0;


    uint32_t esize =
        1u << len;

    uint64_t element =
        (1ULL << (S + 1)) - 1;


    /*
     * Cria máscara para limitar o elemento
     * ao tamanho esize.
     */
    uint64_t element_mask;

    if (esize == 64)
        element_mask = UINT64_MAX;
    else
        element_mask = (1ULL << esize) - 1;

    R %= esize;

    if (R != 0)
    {
        element =
            ((element >> R) |
             (element << (esize - R)))
            & element_mask;
    }

    uint64_t result = 0;
    for (uint32_t pos = 0;
         pos < 64;
         pos += esize)
    {
        result |= element << pos;
    }

    *mask_out = result;
    return 1;
}

void dump_registers(void)
{
    for (int i = 0; i < REG_COUNT; i++)
    {
        printf(
            "X%-2d = 0x%016llx\n",
            i,
            (unsigned long long)reg[i]
        );
    }

    printf(
        "PC  = 0x%016llx\n",
        (unsigned long long)pc
    );

    printf(
        "NZCV = %d%d%d%d\n",
        !!(nzcv & FLAG_N),
        !!(nzcv & FLAG_Z),
        !!(nzcv & FLAG_C),
        !!(nzcv & FLAG_V)
    );
}

void update_logical_flags(uint64_t result)
{
    nzcv &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);

    if (result == 0)
        nzcv |= FLAG_Z;

    if (result >> 63)
        nzcv |= FLAG_N;
}
void update_add_flags(
    uint64_t a,
    uint64_t b,
    uint64_t result)
{
    nzcv &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);

    /* N */
    if (result >> 63)
        nzcv |= FLAG_N;

    /* Z */
    if (result == 0)
        nzcv |= FLAG_Z;

    /* C: carry unsigned */
    if (result < a)
        nzcv |= FLAG_C;

    /* V: overflow signed */
    if ((~(a ^ b) & (a ^ result)) >> 63)
        nzcv |= FLAG_V;
}
void update_sub_flags(
    uint64_t a,
    uint64_t b,
    uint64_t result)
{
    nzcv &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);

    /* N */
    if (result >> 63)
        nzcv |= FLAG_N;

    /* Z */
    if (result == 0)
        nzcv |= FLAG_Z;

    /*
     * C em subtração:
     * 1 = NÃO houve borrow
     * 0 = houve borrow
     */
    if (a >= b)
        nzcv |= FLAG_C;

    /* V: overflow signed */
    if (((a ^ b) & (a ^ result)) >> 63)
        nzcv |= FLAG_V;
}
int condition_holds(uint32_t cond)
{
    int N = (nzcv & FLAG_N) != 0;
    int Z = (nzcv & FLAG_Z) != 0;
    int C = (nzcv & FLAG_C) != 0;
    int V = (nzcv & FLAG_V) != 0;

    switch (cond)
    {
        case 0x0:      /* EQ */
            return Z;

        case 0x1:      /* NE */
            return !Z;

        case 0x2:      /* CS / HS */
            return C;

        case 0x3:      /* CC / LO */
            return !C;

        case 0x4:      /* MI */
            return N;

        case 0x5:      /* PL */
            return !N;

        case 0x6:      /* VS */
            return V;

        case 0x7:      /* VC */
            return !V;

        case 0x8:      /* HI */
            return C && !Z;

        case 0x9:      /* LS */
            return !C || Z;

        case 0xA:      /* GE */
            return N == V;

        case 0xB:      /* LT */
            return N != V;

        case 0xC:      /* GT */
            return !Z && (N == V);

        case 0xD:      /* LE */
            return Z || (N != V);

        case 0xE:      /* AL */
            return 1;

        case 0xF:
            return 1;
    }

    return 0;
}
int shift_register(
    uint64_t value,
    uint32_t shift_type,
    uint32_t amount,
    int allow_ror,
    uint64_t *result)
{
    amount &= 0x3F;

    switch (shift_type)
    {
        case 0b00: /* LSL */
            *result = value << amount;
            return 1;

        case 0b01: /* LSR */
            *result = value >> amount;
            return 1;

        case 0b10: /* ASR */
            *result =
                (uint64_t)((int64_t)value >> amount);
            return 1;

        case 0b11: /* ROR */
            if (!allow_ror)
                return 0;

            if (amount == 0)
            {
                *result = value;
            }
            else
            {
                *result =
                    (value >> amount) |
                    (value << (64 - amount));
            }

            return 1;
    }

    return 0;
}
int extend_register(
    uint64_t value,
    uint32_t option,
    uint32_t shift,
    uint64_t *result)
{
    /*
     * A64 permite no máximo shift 4
     * nesta forma.
     */
    if (shift > 4)
        return 0;

    uint64_t extended;

    switch (option)
    {
        case 0b000: /* UXTB */
            extended =
                value & 0xFF;
            break;

        case 0b001: /* UXTH */
            extended =
                value & 0xFFFF;
            break;

        case 0b010: /* UXTW */
            extended =
                value & 0xFFFFFFFFULL;
            break;

        case 0b011: /* UXTX */
            extended =
                value;
            break;


        case 0b100: /* SXTB */
            extended =
                (uint64_t)(int64_t)(int8_t)value;
            break;

        case 0b101: /* SXTH */
            extended =
                (uint64_t)(int64_t)(int16_t)value;
            break;

        case 0b110: /* SXTW */
            extended =
                (uint64_t)(int64_t)(int32_t)value;
            break;

        case 0b111: /* SXTX */
            extended =
                (uint64_t)(int64_t)value;
            break;

        default:
            return 0;
    }

    *result =
        extended << shift;

    return 1;
}

int run_binary(const char *path)
{
    memset(
        memory,
        0,
        sizeof(memory)
    );

    memset(
        reg,
        0,
        sizeof(reg)
    );


    nzcv = 0;

	const uint64_t PROGRAM_START = 0x1000;
	pc = PROGRAM_START;

    size_t program_size =
        load_image(
            path,
            PROGRAM_START
        );


    if (program_size == 0)
    {
        fprintf(
            stderr,
            "failed to load image: %s\n",
            path
        );

        return 0;
    }

	uint64_t program_end =
		PROGRAM_START +
		program_size;

    /* ----------------------------------------
     * Fetch -> Decode -> Execute
     * ----------------------------------------
     */

    int running = 1;

    while (running && pc<program_end)
    {
        uint32_t instr = mem_read32(pc);
        pc += 4;

        enum opcode op =
            decode_instruction(instr);

        switch (op)
        {
            case OP_ADD_IMM:
            {
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				uint64_t imm12 = (instr >> 10) & 0xFFF;
				uint32_t sh = (instr >> 22) & 0x1;

				if (sh)
				{
					imm12 <<= 12;
				} 

				reg[Rd] = reg[Rn] + imm12;
				break;
            }


			case OP_ADDS_IMM:
			{
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				uint64_t imm12 = (instr >> 10) & 0xFFF;
				uint32_t sh = (instr >> 22) & 0x1;

				if (sh)
					imm12 <<= 12;

				uint64_t a = reg[Rn];
				uint64_t b = imm12;

				uint64_t result = a + b;

				reg[Rd] = result;

				update_add_flags(a, b, result);

				break;
			}

			case OP_SUB_IMM:
			{
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				uint64_t imm12 = (instr >> 10) & 0xFFF;
				uint32_t sh = (instr >> 22) & 0x1;

				if (sh)
				{
					imm12 <<= 12;
				} 

				reg[Rd] = reg[Rn] - imm12;
				break;
			}

			case OP_SUBS_IMM:
			{
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				uint64_t imm12 = (instr >> 10) & 0xFFF;
				uint32_t sh = (instr >> 22) & 0x1;

				if (sh)
					imm12 <<= 12;

				uint64_t a = reg[Rn];
				uint64_t b = imm12;

				uint64_t result = a - b;

				reg[Rd] = result;

				update_sub_flags(a, b, result);

				break;
			}

			case OP_AND_IMM:
			{
				uint32_t N = (instr >> 22) & 0x1;

				uint32_t immr = (instr >> 16) & 0x3F;

				uint32_t imms = (instr >> 10) & 0x3F;

				uint32_t Rn = (instr >> 5) & 0x1F;

				uint32_t Rd = instr & 0x1F;

				uint64_t immediate_mask;

				if (!build_logical_immediate_mask(
						N,
						immr,
						imms,
						&immediate_mask))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] & immediate_mask;

				break;
			}
			case OP_ANDS_IMM:
			{
				uint32_t N = (instr >> 22) & 0x1;
				uint32_t immr = (instr >> 16) & 0x3F;
				uint32_t imms = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				uint64_t immediate_mask;

				if (!build_logical_immediate_mask(
						N,
						immr,
						imms,
						&immediate_mask))
				{
					running = 0;
					break;
				}

				uint64_t result =
					reg[Rn] & immediate_mask;

				reg[Rd] = result;

				update_logical_flags(result);

				break;
			}
			case OP_ORR_IMM:
			{
				uint32_t N = (instr >> 22) & 0x1;
				uint32_t immr = (instr >> 16) & 0x3F;
				uint32_t imms = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				uint64_t immediate_mask;

				if (!build_logical_immediate_mask(
						N,
						immr,
						imms,
						&immediate_mask))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] | immediate_mask;

				break;
			}
			case OP_EOR_IMM:
			{
				uint32_t N = (instr >> 22) & 0x1;
				uint32_t immr = (instr >> 16) & 0x3F;
				uint32_t imms = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				uint64_t immediate_mask;

				if (!build_logical_immediate_mask(
						N,
						immr,
						imms,
						&immediate_mask))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] ^ immediate_mask;

				break;
				}
			case OP_ADD_SHIFT:
			{
					uint32_t shift =
						(instr >> 22) & 0x3;

					uint32_t Rm =
						(instr >> 16) & 0x1F;

					uint32_t imm6 =
						(instr >> 10) & 0x3F;

					uint32_t Rn =
						(instr >> 5) & 0x1F;

					uint32_t Rd =
						instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!shift_register(
						reg[Rm],
						shift,
						imm6,
						0,
						&operand2))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] + operand2;

				break;
			}
			case OP_ADDS_SHIFT:
			{
					uint32_t shift = (instr >> 22) & 0x3;
					uint32_t Rm = (instr >> 16) & 0x1F;
					uint32_t imm6 = (instr >> 10) & 0x3F;
					uint32_t Rn = (instr >> 5) & 0x1F;
					uint32_t Rd = instr & 0x1F;

					if (Rn == 31 || Rm == 31 || Rd == 31)
					{
						running = 0;
						break;
					}

				uint64_t operand2;

				if (!shift_register(
						reg[Rm],
						shift,
						imm6,
						0,
						&operand2))
				{
					running = 0;
					break;
				}

				uint64_t a = reg[Rn];
				uint64_t b = operand2;

				uint64_t result =
					a + b;

				reg[Rd] = result;

				update_add_flags(a, b, result);

				break;
			}
			case OP_MOVZ:
			{
				uint32_t hw =
					(instr >> 21) & 0x3;

				uint64_t imm16 =
					(instr >> 5) & 0xFFFF;

				uint32_t Rd =
					instr & 0x1F;

				uint32_t shift =
					hw * 16;

				reg[Rd] =
					imm16 << shift;

				break;
			}
			case OP_MOVN:
			{
				uint32_t hw =
					(instr >> 21) & 0x3;

				uint64_t imm16 =
					(instr >> 5) & 0xFFFF;

				uint32_t Rd =
					instr & 0x1F;

				uint32_t shift =
					hw * 16;

				reg[Rd] =
					~(imm16 << shift);

				break;
			}
			case OP_SUB_SHIFT:
			{
				uint32_t shift = (instr >> 22) & 0x3;
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t imm6 = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!shift_register(
						reg[Rm],
						shift,
						imm6,
						0,
						&operand2))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] - operand2;

				break;
			}
			case OP_MOVK:
			{
				uint32_t hw =
					(instr >> 21) & 0x3;

				uint64_t imm16 =
					(instr >> 5) & 0xFFFF;

				uint32_t Rd =
					instr & 0x1F;

				uint32_t shift =
					hw * 16;

				uint64_t field_mask =
					0xFFFFULL << shift;

				reg[Rd] =
					(reg[Rd] & ~field_mask) |
					(imm16 << shift);

				break;
			}
			case OP_SUBS_SHIFT:
			{
				uint32_t shift = (instr >> 22) & 0x3;
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t imm6 = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!shift_register(
						reg[Rm],
						shift,
						imm6,
						0,
						&operand2))
				{
					running = 0;
					break;
				}

				uint64_t a = reg[Rn];
				uint64_t b = operand2;

				uint64_t result =
					a - b;

				reg[Rd] = result;

				update_sub_flags(a, b, result);

				break;
			}
			case OP_AND_SHIFT:
			{
				uint32_t shift = (instr >> 22) & 0x3;
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t imm6 = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!shift_register(
						reg[Rm],
						shift,
						imm6,
						1,
						&operand2))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] & operand2;

				break;
			}
			case OP_ANDS_SHIFT:
			{
				uint32_t shift = (instr >> 22) & 0x3;
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t imm6 = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!shift_register(
						reg[Rm],
						shift,
						imm6,
						1,
						&operand2))
				{
					running = 0;
					break;
				}

				uint64_t result =
					reg[Rn] & operand2;

				reg[Rd] = result;

				update_logical_flags(result);

				break;
			}
			case OP_ADD_EXT:
			{
				uint32_t Rm =
					(instr >> 16) & 0x1F;

				uint32_t option =
					(instr >> 13) & 0x7;

				uint32_t imm3 =
					(instr >> 10) & 0x7;

				uint32_t Rn =
					(instr >> 5) & 0x1F;

				uint32_t Rd =
					instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!extend_register(
						reg[Rm],
						option,
						imm3,
						&operand2))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] + operand2;

				break;
			}
			case OP_ADDS_EXT:
			{
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t option = (instr >> 13) & 0x7;
				uint32_t imm3 = (instr >> 10) & 0x7;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!extend_register(
						reg[Rm],
						option,
						imm3,
						&operand2))
				{
					running = 0;
					break;
				}

				uint64_t a = reg[Rn];
				uint64_t b = operand2;

				uint64_t result =
					a + b;

				reg[Rd] = result;

				update_add_flags(a, b, result);

				break;
			}
			case OP_SUB_EXT:
			{
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t option = (instr >> 13) & 0x7;
				uint32_t imm3 = (instr >> 10) & 0x7;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!extend_register(
						reg[Rm],
						option,
						imm3,
						&operand2))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] - operand2;

				break;
			}
			case OP_SUBS_EXT:
			{
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t option = (instr >> 13) & 0x7;
				uint32_t imm3 = (instr >> 10) & 0x7;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!extend_register(
						reg[Rm],
						option,
						imm3,
						&operand2))
				{
					running = 0;
					break;
				}

				uint64_t a = reg[Rn];
				uint64_t b = operand2;

				uint64_t result =
					a - b;

				reg[Rd] = result;

				update_sub_flags(a, b, result);

				break;
			}
			case OP_LDR:
			{
				uint32_t imm12 =
					(instr >> 10) & 0xFFF;

				uint32_t Rn =
					(instr >> 5) & 0x1F;

				uint32_t Rt =
					instr & 0x1F;

				if (Rn == 31 || Rt == 31)
				{
					running = 0;
					break;
				}

				uint64_t offset =
					(uint64_t)imm12 << 3;

				uint64_t address =
					reg[Rn] + offset;

				reg[Rt] =
					mem_read64(address);

				break;
			}

			case OP_ORR_SHIFT:
			{
				uint32_t shift = (instr >> 22) & 0x3;
				uint32_t Rm = (instr >> 16) & 0x1F;
				uint32_t imm6 = (instr >> 10) & 0x3F;
				uint32_t Rn = (instr >> 5) & 0x1F;
				uint32_t Rd = instr & 0x1F;

					if (Rn == 31 || Rm == 31 || Rd == 31)
					{
						running = 0;
						break;
					}

					uint64_t operand2;

					if (!shift_register(
							reg[Rm],
							shift,
							imm6,
							1,
							&operand2))
					{
						running = 0;
						break;
					}

					reg[Rd] =
						reg[Rn] | operand2;

					break;
				}


			case OP_EOR_SHIFT:
				{
					uint32_t shift = (instr >> 22) & 0x3;
					uint32_t Rm = (instr >> 16) & 0x1F;
					uint32_t imm6 = (instr >> 10) & 0x3F;
					uint32_t Rn = (instr >> 5) & 0x1F;
					uint32_t Rd = instr & 0x1F;

				if (Rn == 31 || Rm == 31 || Rd == 31)
				{
					running = 0;
					break;
				}

				uint64_t operand2;

				if (!shift_register(
						reg[Rm],
						shift,
						imm6,
						1,
						&operand2))
				{
					running = 0;
					break;
				}

				reg[Rd] =
					reg[Rn] ^ operand2;

				break;
			}
			case OP_STR:
			{
				uint32_t imm12 =
					(instr >> 10) & 0xFFF;

				uint32_t Rn =
					(instr >> 5) & 0x1F;

				uint32_t Rt =
					instr & 0x1F;

				if (Rn == 31 || Rt == 31)
				{
					running = 0;
					break;
				}

				uint64_t offset =
					(uint64_t)imm12 << 3;

				uint64_t address =
					reg[Rn] + offset;

				mem_write64(
					address,
					reg[Rt]
				);

				break;
			}

			case OP_BLR:
			{
				uint32_t Rn =
					(instr >> 5) & 0x1F;

				if (Rn == 31)
				{
					running = 0;
					break;
				}

				/*
				* PC já aponta para próxima instrução.
				* Esse é o endereço de retorno.
				*/
				reg[30] = pc;

				/*
				* Branch indireto.
				*/
				pc = reg[Rn];

				break;
			}
            case OP_B:
			{
				uint32_t imm26 =
					instr & 0x03FFFFFF;

				int64_t offset =
					(int64_t)sign_extend(
						imm26,
						26
					);

				offset *= 4;

				uint64_t current_pc =
					pc - 4;

				pc =
					current_pc + offset;

				break;
			}


            case OP_BL:
			{
				uint32_t imm26 =
					instr & 0x03FFFFFF;

				int64_t offset =
					(int64_t)sign_extend(
						imm26,
						26
					);

				offset *= 4;

				uint64_t current_pc =
					pc - 4;

				/*
				* endereço para onde RET deve voltar
				*/
				reg[30] = pc;

				pc =
					current_pc + offset;

				break;
			}


			case OP_BR:
			{
				uint32_t Rn =
					(instr >> 5) & 0x1F;

				if (Rn == 31)
				{
					running = 0;
					break;
				}

				pc = reg[Rn];

				break;
			}

			case OP_RET:
			{
				uint32_t Rn =
					(instr >> 5) & 0x1F;

				if (Rn == 31)
				{
					running = 0;
					break;
				}

				pc = reg[Rn];

				break;
			}
			case OP_B_COND:
			{
				uint32_t imm19 =
					(instr >> 5) & 0x7FFFF;

				uint32_t cond =
					instr & 0xF;

				if (condition_holds(cond))
				{
					int64_t offset =
						(int64_t)sign_extend(
							imm19,
							19
						);

					offset *= 4;

					uint64_t current_pc =
						pc - 4;

					pc =
						current_pc + offset;
				}

				break;
			}

            case OP_UNKNOWN:
            default:
            {
                fprintf(
                    stderr,
                    "unknown instruction "
                    "0x%08x at PC 0x%llx\n",
                    instr,
                    (unsigned long long)(pc - 4)
                );

                running = 0;

                break;
            }
        }
    }


    return 1;
}