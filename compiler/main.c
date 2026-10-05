#include <stdio.h>
#include "lexer.h"

static const char *token_type_name(TokenType type)
{
    switch (type) {
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_OPERATOR: return "OPERATOR";
        case TOKEN_EOF: return "EOF";
        case TOKEN_INVALID: return "INVALID";
    }

    return "UNKNOWN";
}

static const char *operation_name(OperationType op)
{
    switch (op) {
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_UNKNOWN: return "UNKNOWN";
    }

    return "UNKNOWN";
}

int main(void)
{
    const char *source = "12 + 34 * 2";

    Lexer lexer;
    lexer_init(&lexer, source);

    while (1) {
        Token token = lexer_next(&lexer);

        printf("%s", token_type_name(token.type));

        if (token.length > 0) {
            printf(" \"");
            printf("%.*s", (int)token.length, token.start);
            printf("\"");
        }

        if (token.type == TOKEN_OPERATOR) {
            printf(" %s", operation_name(token.operation));
        }

        printf("\n");

        if (token.type == TOKEN_EOF) break;
    }

    return 0;
}
