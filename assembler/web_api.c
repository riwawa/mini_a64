#include <stddef.h>
#include <stdint.h>
#include "lexer.h"
#include "parser.h"
#include "assembler.h"
#include "symbol_table.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

#define MAX_INSTRUCTIONS 4096

static uint32_t output[MAX_INSTRUCTIONS];
static size_t instruction_count;

EXPORT int mini_assemble(const char *source){
    Lexer lexer;
    Parser parser;
    SymbolTable symbols;

    instruction_count=0;
    symbol_table_init(&symbols);

    lexer_init(&lexer,source);
    parser_init(&parser,&lexer);

    if(!assembler_first_pass(&parser,&symbols))
        return 0;

    lexer_init(&lexer,source);
    parser_init(&parser,&lexer);

    if(!assembler_second_pass(
        &parser,
        &symbols,
        output,
        MAX_INSTRUCTIONS,
        &instruction_count
    )) return 0;

    return 1;
}

EXPORT int mini_instruction_count(void){
    return (int)instruction_count;
}

EXPORT uint32_t mini_instruction_word(int index){
    if(index<0 || (size_t)index>=instruction_count)
        return 0;

    return output[index];
}
