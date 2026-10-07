#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "symbol_table.h"
#include "emitter.h"

typedef struct{
    Lexer lexer;
    Token current;
    SymbolTable symbols;
    Emitter emitter;
    int temp_depth;
} Parser;

void parser_init(Parser *parser, const char *source);
void parser_advance(Parser *parser);
int parse_program(Parser *parser);

#endif
