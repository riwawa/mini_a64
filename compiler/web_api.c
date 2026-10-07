#include "parser.h"
#include "emitter.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

static Parser parser;

EXPORT const char *mini_compile(const char *source){
    parser_init(&parser,source);

    if(!parse_program(&parser))
        return NULL;

    return emitter_output(&parser.emitter);
}

EXPORT int mini_symbol_count(void){
    return (int)parser.symbols.count;
}

EXPORT const char *mini_symbol_name(int index){
    if(index<0 || (size_t)index>=parser.symbols.count)
        return NULL;

    return parser.symbols.symbols[index].name;
}

EXPORT int mini_symbol_name_length(int index){
    if(index<0 || (size_t)index>=parser.symbols.count)
        return 0;

    return (int)parser.symbols.symbols[index].length;
}

EXPORT unsigned long long mini_symbol_address(int index){
    if(index<0 || (size_t)index>=parser.symbols.count)
        return 0;

    return parser.symbols.symbols[index].address;
}
