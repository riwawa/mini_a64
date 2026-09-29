#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "lexer.h"
#include "parser.h"
#include "assembler.h"
#include "symbol_table.h"
#include "file_utils.h"

#define MAX_INSTRUCTIONS 4096
int main(
    int argc,
    char **argv)
{
    /*
     * argv[0] = programa
     * argv[1] = input.s
     * argv[2] = output.bin
     */

    if (argc != 3)
    {
        fprintf(
            stderr,
            "usage: %s <input.s> <output.bin>\n",
            argv[0]
        );

        return 1;
    }


    const char *input_filename =
        argv[1];

    const char *output_filename =
        argv[2];


    char *source =
        read_text_file(
            input_filename
        );


    if (source == NULL)
    {
        return 1;
    }


    Lexer lexer;
    Parser parser;

    SymbolTable symbols;


    /* =========================================
     * PASS 1
     * =========================================
     */

    symbol_table_init(
        &symbols
    );

    lexer_init(
        &lexer,
        source
    );

    parser_init(
        &parser,
        &lexer
    );


    if (!assembler_first_pass(
            &parser,
            &symbols))
    {
        fprintf(
            stderr,
            "first pass failed\n"
        );

        free(source);

        return 1;
    }


    /* =========================================
     * PASS 2
     * =========================================
     */

    lexer_init(
        &lexer,
        source
    );

    parser_init(
        &parser,
        &lexer
    );


    uint32_t output[
        MAX_INSTRUCTIONS
    ];

    size_t instruction_count;


    if (!assembler_second_pass(
            &parser,
            &symbols,
            output,
            MAX_INSTRUCTIONS,
            &instruction_count))
    {
        fprintf(
            stderr,
            "second pass failed\n"
        );

        free(source);

        return 1;
    }


    /* =========================================
     * WRITE BINARY
     * =========================================
     */

    if (!write_binary(
            output_filename,
            output,
            instruction_count))
    {
        fprintf(
            stderr,
            "could not write binary\n"
        );

        free(source);

        return 1;
    }


    printf(
        "assembled %zu instructions -> %s\n",
        instruction_count,
        output_filename
    );


    free(source);

    return 0;
}