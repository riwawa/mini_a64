#include "symbol_table.h"

#include <string.h>

void symbol_table_init(
    SymbolTable *table)
{
    table->count = 0;
}


static int symbol_name_equals(
    const Symbol *symbol,
    const char *name,
    size_t length)
{
    if (symbol->length != length)
        return 0;

    return memcmp(
        symbol->name,
        name,
        length
    ) == 0;
}

int symbol_table_find(
    const SymbolTable *table,
    const char *name,
    size_t length,
    uint64_t *address)
{
    for (
        size_t i = 0;
        i < table->count;
        i++
    )
    {
        const Symbol *symbol =
            &table->symbols[i];


        if (symbol_name_equals(
                symbol,
                name,
                length))
        {

            if (address != NULL)
            {
                *address =
                    symbol->address;
            }

            return 1;
        }
    }

    return 0;
}


int symbol_table_add(
    SymbolTable *table,
    const char *name,
    size_t length,
    uint64_t address)
{

    if (symbol_table_find(
            table,
            name,
            length,
            NULL))
    {
        return 0;
    }


    /*
     * Tabela cheia.
     */
    if (table->count >= MAX_SYMBOLS)
    {
        return 0;
    }


    /*
     * Pegamos a próxima posição livre.
     */
    Symbol *symbol =
        &table->symbols[
            table->count
        ];


    symbol->name =
        name;

    symbol->length =
        length;

    symbol->address =
        address;

    table->count++;


    return 1;
}