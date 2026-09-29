#include "parser.h"

#include <stdio.h>
#include <string.h>

static void parser_advance(Parser *parser)
{
    parser->previous = parser->current;
    parser->current = lexer_next(parser->lexer);
}


static int check(
    Parser *parser,
    TokenType type)
{
    return parser->current.type == type;
}


static int match(
    Parser *parser,
    TokenType type)
{
    if (!check(parser, type))
        return 0;

    parser_advance(parser);

    return 1;
}

static void parser_error(
    Parser *parser,
    const char *message)
{
    fprintf(
        stderr,
        "parser error at line %zu: %s\n",
        parser->lexer->line,
        message
    );

    parser->had_error = 1;
}


static Token consume(
    Parser *parser,
    TokenType expected,
    const char *message)
{
    Token token = parser->current;

    if (!check(parser, expected))
    {
        parser_error(
            parser,
            message
        );

        return token;
    }

    parser_advance(parser);

    return token;
}


static int token_equals(
    Token token,
    const char *text)
{
    size_t text_length = strlen(text);

    if (token.length != text_length)
        return 0;

    return memcmp(
        token.start,
        text,
        text_length
    ) == 0;
}

static Instruction invalid_instruction(void)
{
    Instruction instr = {0};

    instr.opcode = OP_UNKNOWN;

    return instr;
}


static int valid_register(
    Parser *parser,
    Token token)
{
    if (token.number > 30)
    {
        parser_error(
            parser,
            "register must be X0-X30"
        );

        return 0;
    }

    return 1;
}

static int parse_shift_type(
    Token token,
    uint32_t *shift_type)
{
    if (token_equals(token, "LSL"))
    {
        *shift_type = 0;
        return 1;
    }

    if (token_equals(token, "LSR"))
    {
        *shift_type = 1;
        return 1;
    }

    if (token_equals(token, "ASR"))
    {
        *shift_type = 2;
        return 1;
    }

    return 0;
}

static int parse_extend_type(
    Token token,
    uint32_t *extend_type)
{
    if (token_equals(token, "UXTB"))
    {
        *extend_type = 0;
        return 1;
    }

    if (token_equals(token, "UXTH"))
    {
        *extend_type = 1;
        return 1;
    }

    if (token_equals(token, "UXTW"))
    {
        *extend_type = 2;
        return 1;
    }

    if (token_equals(token, "UXTX"))
    {
        *extend_type = 3;
        return 1;
    }

    if (token_equals(token, "SXTB"))
    {
        *extend_type = 4;
        return 1;
    }

    if (token_equals(token, "SXTH"))
    {
        *extend_type = 5;
        return 1;
    }

    if (token_equals(token, "SXTW"))
    {
        *extend_type = 6;
        return 1;
    }

    if (token_equals(token, "SXTX"))
    {
        *extend_type = 7;
        return 1;
    }

    return 0;
}


static Instruction parse_add_sub(
    Parser *parser,
    Opcode imm_opcode,
    Opcode shift_opcode,
    Opcode ext_opcode)
{
    Instruction instr = {0};


    /* Rd */

    Token rd =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected destination register"
        );

    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after destination register"
    );


    /* Rn */

    Token rn =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected source register"
        );

    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after source register"
    );


    if (parser->had_error)
        return invalid_instruction();


    if (!valid_register(parser, rd) ||
        !valid_register(parser, rn))
    {
        return invalid_instruction();
    }


    instr.rd = (uint32_t)rd.number;
    instr.rn = (uint32_t)rn.number;


    /* =====================================================
     * IMMEDIATE
     *
     * ADD X1, X0, #5
     * =====================================================
     */

    if (match(
            parser,
            TOKEN_HASH))
    {
        Token immediate =
            consume(
                parser,
                TOKEN_NUMBER,
                "expected immediate value"
            );

        if (parser->had_error)
            return invalid_instruction();


        instr.opcode = imm_opcode;
        instr.immediate = immediate.number;

        return instr;
    }


    /* =====================================================
     * REGISTER
     * =====================================================
     */

    Token rm =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected immediate or register operand"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (!valid_register(parser, rm))
        return invalid_instruction();


    instr.rm = (uint32_t)rm.number;


    /* =====================================================
     * Sem modificador:
     *
     * ADD X1, X0, X2
     *
     * equivale a:
     *
     * ADD X1, X0, X2, LSL #0
     * =====================================================
     */

    if (
        check(parser, TOKEN_NEWLINE) ||
        check(parser, TOKEN_EOF)
    )
    {
        instr.opcode = shift_opcode;
        instr.shift_type = 0;
        instr.shift_amount = 0;

        return instr;
    }


    /* =====================================================
     * Temos modificador
     *
     * ADD X1, X0, X2, ...
     * =====================================================
     */

    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after second source register"
    );


    Token modifier =
        consume(
            parser,
            TOKEN_IDENTIFIER,
            "expected shift or extend modifier"
        );


    if (parser->had_error)
        return invalid_instruction();


    /* =====================================================
     * SHIFTED
     * =====================================================
     */

    uint32_t shift_type;

    if (parse_shift_type(
            modifier,
            &shift_type))
    {
        consume(
            parser,
            TOKEN_HASH,
            "expected '#' before shift amount"
        );


        Token amount =
            consume(
                parser,
                TOKEN_NUMBER,
                "expected shift amount"
            );


        if (parser->had_error)
            return invalid_instruction();


        /*
         * 64-bit shifted register:
         * amount 0-63
         */
        if (amount.number > 63)
        {
            parser_error(
                parser,
                "shift amount must be 0-63"
            );

            return invalid_instruction();
        }


        instr.opcode = shift_opcode;
        instr.shift_type = shift_type;
        instr.shift_amount =
            (uint32_t)amount.number;


        return instr;
    }


    /* =====================================================
     * EXTENDED
     * =====================================================
     */

    uint32_t extend_type;

    if (parse_extend_type(
            modifier,
            &extend_type))
    {
        instr.opcode = ext_opcode;
        instr.extend_type = extend_type;

        /*
         * Default:
         *
         * UXTX
         *
         * = UXTX #0
         */
        instr.shift_amount = 0;


        if (match(
                parser,
                TOKEN_HASH))
        {
            Token amount =
                consume(
                    parser,
                    TOKEN_NUMBER,
                    "expected extend shift amount"
                );


            if (parser->had_error)
                return invalid_instruction();

            if (amount.number > 4)
            {
                parser_error(
                    parser,
                    "extended shift amount must be 0-4"
                );

                return invalid_instruction();
            }


            instr.shift_amount =
                (uint32_t)amount.number;
        }


        return instr;
    }


    parser_error(
        parser,
        "unknown shift or extend modifier"
    );

    return invalid_instruction();
}


static int parse_logical_shift_type(
    Token token,
    uint32_t *shift_type)
{
    if (token_equals(token, "LSL"))
    {
        *shift_type = 0;
        return 1;
    }

    if (token_equals(token, "LSR"))
    {
        *shift_type = 1;
        return 1;
    }

    if (token_equals(token, "ASR"))
    {
        *shift_type = 2;
        return 1;
    }

    if (token_equals(token, "ROR"))
    {
        *shift_type = 3;
        return 1;
    }

    return 0;
}


static Instruction parse_logical(
    Parser *parser,
    Opcode imm_opcode,
    Opcode shift_opcode)
{
    Instruction instr = {0};


    Token rd =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected destination register"
        );


    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after destination register"
    );


    Token rn =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected source register"
        );


    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after source register"
    );


    if (parser->had_error)
        return invalid_instruction();


    if (!valid_register(parser, rd) ||
        !valid_register(parser, rn))
    {
        return invalid_instruction();
    }


    instr.rd = (uint32_t)rd.number;
    instr.rn = (uint32_t)rn.number;


    /* =====================================================
     * IMMEDIATE
     * =====================================================
     */

    if (match(
            parser,
            TOKEN_HASH))
    {
        Token immediate =
            consume(
                parser,
                TOKEN_NUMBER,
                "expected immediate value"
            );


        if (parser->had_error)
            return invalid_instruction();


        instr.opcode = imm_opcode;
        instr.immediate = immediate.number;

        return instr;
    }


    /* =====================================================
     * SHIFTED REGISTER
     * =====================================================
     */

    Token rm =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected register or immediate"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (!valid_register(parser, rm))
        return invalid_instruction();


    instr.opcode = shift_opcode;

    instr.rm =
        (uint32_t)rm.number;

    /*
     * Default:
     *
     * LSL #0
     */
    instr.shift_type = 0;
    instr.shift_amount = 0;


    if (
        check(parser, TOKEN_NEWLINE) ||
        check(parser, TOKEN_EOF)
    )
    {
        return instr;
    }


    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after second source register"
    );


    Token modifier =
        consume(
            parser,
            TOKEN_IDENTIFIER,
            "expected shift modifier"
        );


    if (parser->had_error)
        return invalid_instruction();


    uint32_t shift_type;

    if (!parse_logical_shift_type(
            modifier,
            &shift_type))
    {
        parser_error(
            parser,
            "unknown logical shift"
        );

        return invalid_instruction();
    }


    consume(
        parser,
        TOKEN_HASH,
        "expected '#' before shift amount"
    );


    Token amount =
        consume(
            parser,
            TOKEN_NUMBER,
            "expected shift amount"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (amount.number > 63)
    {
        parser_error(
            parser,
            "shift amount must be 0-63"
        );

        return invalid_instruction();
    }


    instr.shift_type = shift_type;
    instr.shift_amount =
        (uint32_t)amount.number;


    return instr;
}


static Instruction parse_move_wide(
    Parser *parser,
    Opcode opcode)
{
    Instruction instr = {0};

    instr.opcode = opcode;


    Token rd =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected destination register"
        );


    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after destination register"
    );


    consume(
        parser,
        TOKEN_HASH,
        "expected '#' before immediate"
    );


    Token immediate =
        consume(
            parser,
            TOKEN_NUMBER,
            "expected immediate value"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (!valid_register(parser, rd))
        return invalid_instruction();


    /*
     * imm16
     */
    if (immediate.number > 0xFFFF)
    {
        parser_error(
            parser,
            "MOV immediate must fit in 16 bits"
        );

        return invalid_instruction();
    }


    instr.rd =
        (uint32_t)rd.number;

    instr.immediate =
        immediate.number;

    instr.shift_amount = 0;


    /*
     * MOVZ X0, #5
     */
    if (
        check(parser, TOKEN_NEWLINE) ||
        check(parser, TOKEN_EOF)
    )
    {
        return instr;
    }


    /*
     * MOVZ X0, #5, LSL #16
     */
    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' before shift"
    );


    Token modifier =
        consume(
            parser,
            TOKEN_IDENTIFIER,
            "expected LSL"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (!token_equals(
            modifier,
            "LSL"))
    {
        parser_error(
            parser,
            "move wide only supports LSL"
        );

        return invalid_instruction();
    }


    consume(
        parser,
        TOKEN_HASH,
        "expected '#' before shift amount"
    );


    Token amount =
        consume(
            parser,
            TOKEN_NUMBER,
            "expected shift amount"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (
        amount.number != 0 &&
        amount.number != 16 &&
        amount.number != 32 &&
        amount.number != 48
    )
    {
        parser_error(
            parser,
            "MOV shift must be 0, 16, 32 or 48"
        );

        return invalid_instruction();
    }


    instr.shift_amount =
        (uint32_t)amount.number;


    return instr;
}


static Instruction parse_load_store(
    Parser *parser,
    Opcode opcode)
{
    Instruction instr = {0};

    instr.opcode = opcode;


    Token rt =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected register"
        );


    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after register"
    );


    consume(
        parser,
        TOKEN_LBRACKET,
        "expected '['"
    );


    Token rn =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected base register"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (!valid_register(parser, rt) ||
        !valid_register(parser, rn))
    {
        return invalid_instruction();
    }


    instr.rd =
        (uint32_t)rt.number;

    instr.rn =
        (uint32_t)rn.number;

    instr.immediate = 0;


    /*
     * [X0]
     */
    if (match(
            parser,
            TOKEN_RBRACKET))
    {
        return instr;
    }


    /*
     * [X0, #8]
     */
    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after base register"
    );


    consume(
        parser,
        TOKEN_HASH,
        "expected '#' before offset"
    );


    Token offset =
        consume(
            parser,
            TOKEN_NUMBER,
            "expected memory offset"
        );


    consume(
        parser,
        TOKEN_RBRACKET,
        "expected ']'"
    );


    if (parser->had_error)
        return invalid_instruction();


    instr.immediate =
        offset.number;


    return instr;
}

static Instruction parse_branch_register(
    Parser *parser,
    Opcode opcode)
{
    Instruction instr = {0};

    instr.opcode = opcode;


    /*
     * RET
     *
     * equivale a:
     *
     * RET X30
     */
    if (
        opcode == OP_RET &&
        (
            check(parser, TOKEN_NEWLINE) ||
            check(parser, TOKEN_EOF)
        )
    )
    {
        instr.rn = 30;

        return instr;
    }


    Token rn =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected branch register"
        );


    if (parser->had_error)
        return invalid_instruction();


    if (!valid_register(parser, rn))
        return invalid_instruction();


    instr.rn =
        (uint32_t)rn.number;


    return instr;
}

static Instruction parse_branch_label(
    Parser *parser,
    Opcode opcode)
{
    Instruction instr = {0};

    instr.opcode = opcode;


    Token label =
        consume(
            parser,
            TOKEN_IDENTIFIER,
            "expected branch label"
        );


    if (parser->had_error)
        return invalid_instruction();


    instr.label_start =
        label.start;

    instr.label_length =
        label.length;


    return instr;
}


/* =========================================================
 * B.cond
 *
 * condition codes A64
 * =========================================================
 */

static int parse_condition(
    Token mnemonic,
    uint32_t *condition)
{
    if (token_equals(mnemonic, "B.EQ"))
    {
        *condition = 0x0;
        return 1;
    }

    if (token_equals(mnemonic, "B.NE"))
    {
        *condition = 0x1;
        return 1;
    }

    if (
        token_equals(mnemonic, "B.CS") ||
        token_equals(mnemonic, "B.HS")
    )
    {
        *condition = 0x2;
        return 1;
    }

    if (
        token_equals(mnemonic, "B.CC") ||
        token_equals(mnemonic, "B.LO")
    )
    {
        *condition = 0x3;
        return 1;
    }

    if (token_equals(mnemonic, "B.MI"))
    {
        *condition = 0x4;
        return 1;
    }

    if (token_equals(mnemonic, "B.PL"))
    {
        *condition = 0x5;
        return 1;
    }

    if (token_equals(mnemonic, "B.VS"))
    {
        *condition = 0x6;
        return 1;
    }

    if (token_equals(mnemonic, "B.VC"))
    {
        *condition = 0x7;
        return 1;
    }

    if (token_equals(mnemonic, "B.HI"))
    {
        *condition = 0x8;
        return 1;
    }

    if (token_equals(mnemonic, "B.LS"))
    {
        *condition = 0x9;
        return 1;
    }

    if (token_equals(mnemonic, "B.GE"))
    {
        *condition = 0xA;
        return 1;
    }

    if (token_equals(mnemonic, "B.LT"))
    {
        *condition = 0xB;
        return 1;
    }

    if (token_equals(mnemonic, "B.GT"))
    {
        *condition = 0xC;
        return 1;
    }

    if (token_equals(mnemonic, "B.LE"))
    {
        *condition = 0xD;
        return 1;
    }

    return 0;
}


static Instruction parse_cond_branch(
    Parser *parser,
    uint32_t condition)
{
    Instruction instr = {0};

    instr.opcode = OP_B_COND;
    instr.condition = condition;


    Token label =
        consume(
            parser,
            TOKEN_IDENTIFIER,
            "expected branch label"
        );


    if (parser->had_error)
        return invalid_instruction();


    instr.label_start =
        label.start;

    instr.label_length =
        label.length;


    return instr;
}


/* =========================================================
 * INIT
 * =========================================================
 */

void parser_init(
    Parser *parser,
    Lexer *lexer)
{
    parser->lexer = lexer;
    parser->had_error = 0;


    memset(
        &parser->current,
        0,
        sizeof(Token)
    );


    memset(
        &parser->previous,
        0,
        sizeof(Token)
    );


    parser->current =
        lexer_next(lexer);
}

/* =========================================================
 * STATEMENT INVÁLIDO
 * =========================================================
 */

static Statement invalid_statement(void)
{
    Statement stmt = {0};

    stmt.type = STATEMENT_INVALID;

    return stmt;
}

static Instruction parse_instruction_after_mnemonic(
    Parser *parser,
    Token mnemonic)
{
    /* =====================================================
     * ADD / ADDS / SUB / SUBS
     * =====================================================
     */

    if (token_equals(mnemonic, "ADD"))
    {
        return parse_add_sub(
            parser,
            OP_ADD_IMM,
            OP_ADD_SHIFT,
            OP_ADD_EXT
        );
    }


    if (token_equals(mnemonic, "ADDS"))
    {
        return parse_add_sub(
            parser,
            OP_ADDS_IMM,
            OP_ADDS_SHIFT,
            OP_ADDS_EXT
        );
    }


    if (token_equals(mnemonic, "SUB"))
    {
        return parse_add_sub(
            parser,
            OP_SUB_IMM,
            OP_SUB_SHIFT,
            OP_SUB_EXT
        );
    }


    if (token_equals(mnemonic, "SUBS"))
    {
        return parse_add_sub(
            parser,
            OP_SUBS_IMM,
            OP_SUBS_SHIFT,
            OP_SUBS_EXT
        );
    }


    /* =====================================================
     * LOGICAL
     * =====================================================
     */

    if (token_equals(mnemonic, "AND"))
    {
        return parse_logical(
            parser,
            OP_AND_IMM,
            OP_AND_SHIFT
        );
    }


    if (token_equals(mnemonic, "ANDS"))
    {
        return parse_logical(
            parser,
            OP_ANDS_IMM,
            OP_ANDS_SHIFT
        );
    }


    if (token_equals(mnemonic, "ORR"))
    {
        return parse_logical(
            parser,
            OP_ORR_IMM,
            OP_ORR_SHIFT
        );
    }


    if (token_equals(mnemonic, "EOR"))
    {
        return parse_logical(
            parser,
            OP_EOR_IMM,
            OP_EOR_SHIFT
        );
    }


    /* =====================================================
     * MOV WIDE
     * =====================================================
     */

    if (token_equals(mnemonic, "MOVZ"))
    {
        return parse_move_wide(
            parser,
            OP_MOVZ
        );
    }


    if (token_equals(mnemonic, "MOVN"))
    {
        return parse_move_wide(
            parser,
            OP_MOVN
        );
    }


    if (token_equals(mnemonic, "MOVK"))
    {
        return parse_move_wide(
            parser,
            OP_MOVK
        );
    }


    /* =====================================================
     * LOAD / STORE
     * =====================================================
     */

    if (token_equals(mnemonic, "LDR"))
    {
        return parse_load_store(
            parser,
            OP_LDR
        );
    }


    if (token_equals(mnemonic, "STR"))
    {
        return parse_load_store(
            parser,
            OP_STR
        );
    }


    /* =====================================================
     * B / BL
     * =====================================================
     */

    if (token_equals(mnemonic, "B"))
    {
        return parse_branch_label(
            parser,
            OP_B
        );
    }


    if (token_equals(mnemonic, "BL"))
    {
        return parse_branch_label(
            parser,
            OP_BL
        );
    }


    /* =====================================================
     * B.COND
     * =====================================================
     */

    uint32_t condition;

    if (parse_condition(
            mnemonic,
            &condition))
    {
        return parse_cond_branch(
            parser,
            condition
        );
    }


    /* =====================================================
     * REGISTER BRANCH
     * =====================================================
     */

    if (token_equals(mnemonic, "BR"))
    {
        return parse_branch_register(
            parser,
            OP_BR
        );
    }


    if (token_equals(mnemonic, "BLR"))
    {
        return parse_branch_register(
            parser,
            OP_BLR
        );
    }


    if (token_equals(mnemonic, "RET"))
    {
        return parse_branch_register(
            parser,
            OP_RET
        );
    }


    /* =====================================================
     * UNKNOWN
     * =====================================================
     */

    parser_error(
        parser,
        "unknown instruction"
    );

    return invalid_instruction();
}

Statement parser_next_statement(
    Parser *parser)
{
    Statement stmt = {0};

    /*
     * Novo statement:
     * limpa erro anterior.
     */
    parser->had_error = 0;


    /* =====================================================
     * Ignora linhas vazias
     * =====================================================
     */

    while (match(
        parser,
        TOKEN_NEWLINE))
    {
        /* nada */
    }


    /* =====================================================
     * EOF
     * =====================================================
     */

    if (check(
        parser,
        TOKEN_EOF))
    {
        stmt.type =
            STATEMENT_EOF;

        return stmt;
    }

    Token first =
        consume(
            parser,
            TOKEN_IDENTIFIER,
            "expected instruction or label"
        );


    if (parser->had_error)
    {
        return invalid_statement();
    }


    if (match(
            parser,
            TOKEN_COLON))
    {
        stmt.type =
            STATEMENT_LABEL;

        stmt.label_start =
            first.start;

        stmt.label_length =
            first.length;

        if (
            !check(parser, TOKEN_NEWLINE) &&
            !check(parser, TOKEN_EOF)
        )
        {
            parser_error(
                parser,
                "expected end of line after label"
            );

            return invalid_statement();
        }


        return stmt;
    }

    stmt.type =
        STATEMENT_INSTRUCTION;


    stmt.instruction =
        parse_instruction_after_mnemonic(
            parser,
            first
        );


    if (parser->had_error)
    {
        stmt.type =
            STATEMENT_INVALID;

        return stmt;
    }

    if (
        !check(parser, TOKEN_NEWLINE) &&
        !check(parser, TOKEN_EOF)
    )
    {
        parser_error(
            parser,
            "unexpected token after instruction"
        );

        stmt.type =
            STATEMENT_INVALID;

        return stmt;
    }


    return stmt;
}