#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "instruction.h"
#include "statement.h"


typedef struct
{
    Lexer *lexer;

    Token current;
    Token previous;

    int had_error;

} Parser;


void parser_init(
    Parser *parser,
    Lexer *lexer
);


Statement parser_next_statement(
    Parser *parser
);


#endif