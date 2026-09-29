#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <stddef.h>


typedef enum
{
    TOKEN_EOF,
    TOKEN_INVALID,

    TOKEN_IDENTIFIER,
    TOKEN_REGISTER,
    TOKEN_NUMBER,

    TOKEN_COMMA,
    TOKEN_HASH,
    TOKEN_COLON,

    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    TOKEN_NEWLINE

} TokenType;


typedef struct
{
    TokenType type;

    const char *start;
    size_t length;

    uint64_t number;

} Token;


typedef struct
{
    const char *source;

    size_t pos;
    size_t line;

} Lexer;


void lexer_init(
    Lexer *lexer,
    const char *source
);


Token lexer_next(
    Lexer *lexer
);


#endif