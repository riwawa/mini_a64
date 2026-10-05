#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>
#include <stdint.h>


#define MAX_SYMBOLS 256


typedef struct
{
    const char *name;
    size_t length;
    uint64_t address;
} Symbol;


typedef struct
{
    Symbol symbols[MAX_SYMBOLS];
    size_t count;
} SymbolTable;


void symbol_table_init(
    SymbolTable *table
);

int symbol_table_add(
    SymbolTable *table,
    const char *name,
    size_t length,
    uint64_t address
);


int symbol_table_find(
    const SymbolTable *table,
    const char *name,
    size_t length,
    uint64_t *address
);


#endif