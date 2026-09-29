#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "parser.h"
#include "symbol_table.h"


int assembler_first_pass(
    Parser *parser,
    SymbolTable *symbols
);

int assembler_second_pass(
    Parser *parser,
    const SymbolTable *symbols,
    uint32_t *output,
    size_t output_capacity,
    size_t *output_count
);

int write_binary(
    const char *filename,
    const uint32_t *instructions,
    size_t instruction_count
);

#endif
