#include "lexer.h"

static char peek(Lexer *lexer)
{
    return lexer->source[lexer->pos];
}

static char advance(Lexer *lexer)
{
    return lexer->source[lexer->pos++];
}

static int is_at_end(Lexer *lexer)
{
    return peek(lexer) == '\0';
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static OperationType get_operation_type(char c)
{
    switch (c) {
        case '+': return OP_ADD;
        case '-': return OP_SUB;
        case '*': return OP_MUL;
        case '/': return OP_DIV;
        default: return OP_UNKNOWN;
    }
}

static Token make_token(TokenType type, const char *start, size_t length)
{
    Token token;
    token.type = type;
    token.start = start;
    token.length = length;
    token.operation = OP_UNKNOWN;
    return token;
}

static Token make_operator_token(OperationType operation, const char *start, size_t length)
{
    Token token = make_token(TOKEN_OPERATOR, start, length);
    token.operation = operation;
    return token;
}

static Token read_number(Lexer *lexer, size_t start)
{
    while (is_digit(peek(lexer))) advance(lexer);

    return make_token(
        TOKEN_NUMBER,
        &lexer->source[start],
        lexer->pos - start
    );
}

void lexer_init(Lexer *lexer, const char *source)
{
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
}

Token lexer_next(Lexer *lexer)
{
    while (!is_at_end(lexer)) {
        size_t start = lexer->pos;
        char c = advance(lexer);

        if (c == ' ' || c == '\t' || c == '\r') continue;

        if (c == '\n') {
            lexer->line++;
            continue;
        }

        if (is_digit(c)) {
            lexer->pos = start;
            return read_number(lexer, start);
        }

        OperationType operation = get_operation_type(c);

        if (operation != OP_UNKNOWN) {
            return make_operator_token(
                operation,
                &lexer->source[start],
                1
            );
        }

        return make_token(
            TOKEN_INVALID,
            &lexer->source[start],
            1
        );
    }

    return make_token(
        TOKEN_EOF,
        &lexer->source[lexer->pos],
        0
    );
}
