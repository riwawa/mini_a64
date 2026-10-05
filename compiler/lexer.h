#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include "compiler_types.h"

typedef enum {
    TOKEN_NUMBER,
    TOKEN_OPERATOR,
    TOKEN_EOF,
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    OperationType operation;
} Token;

typedef struct {
    const char *source;
    size_t pos;
    size_t line;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next(Lexer *lexer);

#endif
