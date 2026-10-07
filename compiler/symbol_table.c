#include <string.h>
#include "symbol_table.h"

void symbol_table_init(SymbolTable *table){
    table->count = 0;
    table->next_address = VARIABLE_BASE_ADDRESS;
}

Symbol *symbol_table_find(SymbolTable *table, const char *name, size_t length){
    for(size_t i = 0; i < table->count; i++){
        Symbol *symbol = &table->symbols[i];

        if(symbol->length == length &&
           strncmp(symbol->name, name, length) == 0)
            return symbol;
    }

    return NULL;
}

Symbol *symbol_table_add(SymbolTable *table, const char *name, size_t length){
    if(table->count >= MAX_SYMBOLS)
        return NULL;

    if(symbol_table_find(table, name, length))
        return NULL;

    Symbol *symbol = &table->symbols[table->count++];

    symbol->name = name;
    symbol->length = length;
    symbol->address = table->next_address;

    table->next_address += 8;

    return symbol;
}
