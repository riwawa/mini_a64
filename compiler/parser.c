#include <stdio.h>
#include <stdint.h>
#include "parser.h"

#define TEMP_FIRST 3
#define TEMP_LAST 29

void parser_advance(Parser *parser){
    parser->current=lexer_next(&parser->lexer);
}

void parser_init(Parser *parser,const char *source){
    lexer_init(&parser->lexer,source);
    symbol_table_init(&parser->symbols);
    emitter_init(&parser->emitter);
    parser->temp_depth=0;
    parser_advance(parser);
}

static uint64_t token_number(Token token){
    uint64_t value=0;
    for(size_t i=0;i<token.length;i++)
        value=value*10+(token.start[i]-'0');
    return value;
}

static int temp_push(Parser *parser){
    int reg=TEMP_FIRST+parser->temp_depth;

    if(reg>TEMP_LAST){
        fprintf(stderr,"expression nesting too deep\n");
        return -1;
    }

    parser->temp_depth++;
    emitter_write(&parser->emitter,"ADD X%d, X0, #0\n",reg);
    return reg;
}

static void temp_pop(Parser *parser){
    if(parser->temp_depth>0) parser->temp_depth--;
}

static int parse_expression(Parser *parser);

static int parse_factor(Parser *parser){
    if(parser->current.type==TOKEN_NUMBER){
        uint64_t value=token_number(parser->current);

        if(value>65535){
            fprintf(stderr,"integer literal too large for MOVZ\n");
            return 0;
        }

        emitter_write(&parser->emitter,"MOVZ X0, #%llu\n",(unsigned long long)value);
        parser_advance(parser);
        return 1;
    }

    if(parser->current.type==TOKEN_IDENTIFIER){
        Symbol *symbol=symbol_table_find(
            &parser->symbols,
            parser->current.start,
            parser->current.length
        );

        if(!symbol){
            fprintf(stderr,"undeclared variable: %.*s\n",
                (int)parser->current.length,
                parser->current.start);
            return 0;
        }

        emitter_write(&parser->emitter,"MOVZ X2, #%llu\n",
            (unsigned long long)symbol->address);
        emitter_write(&parser->emitter,"LDR X0, [X2]\n");

        parser_advance(parser);
        return 1;
    }

    if(parser->current.type==TOKEN_LPAREN){
        parser_advance(parser);

        if(!parse_expression(parser)) return 0;

        if(parser->current.type!=TOKEN_RPAREN){
            fprintf(stderr,"expected ')'\n");
            return 0;
        }

        parser_advance(parser);
        return 1;
    }

    fprintf(stderr,"expected number, identifier or '('\n");
    return 0;
}

static int parse_expression(Parser *parser){
    if(!parse_factor(parser)) return 0;

    while(parser->current.type==TOKEN_OPERATOR){
        OperationType op=parser->current.operation;

        if(op!=OP_ADD && op!=OP_SUB){
            fprintf(stderr,"operator not supported by Mini-A64 v1: %.*s\n",
                (int)parser->current.length,
                parser->current.start);
            return 0;
        }

        int reg=temp_push(parser);
        if(reg<0) return 0;

        parser_advance(parser);

        if(!parse_factor(parser)){
            temp_pop(parser);
            return 0;
        }

        if(op==OP_ADD)
            emitter_write(&parser->emitter,"ADD X0, X%d, X0\n",reg);
        else
            emitter_write(&parser->emitter,"SUB X0, X%d, X0\n",reg);

        temp_pop(parser);
    }

    return 1;
}

static int parse_declaration(Parser *parser){
    parser_advance(parser);

    if(parser->current.type!=TOKEN_IDENTIFIER){
        fprintf(stderr,"expected identifier after let\n");
        return 0;
    }

    Token name=parser->current;

    if(symbol_table_find(&parser->symbols,name.start,name.length)){
        fprintf(stderr,"variable already declared: %.*s\n",
            (int)name.length,name.start);
        return 0;
    }

    if(parser->symbols.count>=MAX_SYMBOLS){
        fprintf(stderr,"symbol table full\n");
        return 0;
    }

    parser_advance(parser);

    if(parser->current.type!=TOKEN_ASSIGN){
        fprintf(stderr,"expected '=' after variable name\n");
        return 0;
    }

    parser_advance(parser);

    if(!parse_expression(parser)) return 0;

    Symbol *symbol=symbol_table_add(
        &parser->symbols,
        name.start,
        name.length
    );

    if(!symbol){
        fprintf(stderr,"could not declare variable\n");
        return 0;
    }

    emitter_write(&parser->emitter,"MOVZ X2, #%llu\n",
        (unsigned long long)symbol->address);
    emitter_write(&parser->emitter,"STR X0, [X2]\n");

    return 1;
}

static int parse_assignment(Parser *parser){
    Token name=parser->current;

    Symbol *symbol=symbol_table_find(
        &parser->symbols,
        name.start,
        name.length
    );

    if(!symbol){
        fprintf(stderr,"undeclared variable: %.*s\n",
            (int)name.length,name.start);
        return 0;
    }

    parser_advance(parser);

    if(parser->current.type!=TOKEN_ASSIGN){
        fprintf(stderr,"expected '=' after variable name\n");
        return 0;
    }

    parser_advance(parser);

    if(!parse_expression(parser)) return 0;

    emitter_write(&parser->emitter,"MOVZ X2, #%llu\n",
        (unsigned long long)symbol->address);
    emitter_write(&parser->emitter,"STR X0, [X2]\n");

    return 1;
}

static int parse_statement(Parser *parser){
    if(parser->current.type==TOKEN_LET)
        return parse_declaration(parser);

    if(parser->current.type==TOKEN_IDENTIFIER)
        return parse_assignment(parser);

    fprintf(stderr,"expected statement: %.*s\n",
        (int)parser->current.length,
        parser->current.start);

    return 0;
}

int parse_program(Parser *parser){
    while(parser->current.type!=TOKEN_EOF){
        while(parser->current.type==TOKEN_NEWLINE)
            parser_advance(parser);

        if(parser->current.type==TOKEN_EOF) break;

        if(!parse_statement(parser)) return 0;

        if(parser->current.type!=TOKEN_NEWLINE &&
           parser->current.type!=TOKEN_EOF){
            fprintf(stderr,"expected newline after statement\n");
            return 0;
        }

        while(parser->current.type==TOKEN_NEWLINE)
            parser_advance(parser);
    }

    return 1;
}
