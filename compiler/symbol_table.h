#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>
#include <stdint.h>

#define MAX_SYMBOLS 64
#define VARIABLE_BASE_ADDRESS 0x2000

typedef struct{
    const char *name;
    size_t length;
    uint64_t address;
} Symbol;

typedef struct{
    Symbol symbols[MAX_SYMBOLS];
    size_t count;
    uint64_t next_address;
} SymbolTable;

void symbol_table_init(SymbolTable *table);
Symbol *symbol_table_find(SymbolTable *table, const char *name, size_t length);
Symbol *symbol_table_add(SymbolTable *table, const char *name, size_t length);

#endif
