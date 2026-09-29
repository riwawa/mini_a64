#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "parser.h"
#include "symbol_table.h"
#include "assembler.h"
#include "encoder.h"

int assembler_first_pass(
    Parser *parser,
    SymbolTable *symbols)
{
    uint64_t pc = 0;

    for (;;)
    {
        Statement stmt =
            parser_next_statement(
                parser
            );

        if (
            stmt.type ==
            STATEMENT_EOF
        )
        {
            break;
        }

        if (
            stmt.type ==
            STATEMENT_INVALID
        )
        {
            return 0;
        }

        if (
            stmt.type ==
            STATEMENT_LABEL
        )
        {
            int ok =
                symbol_table_add(
                    symbols,
                    stmt.label_start,
                    stmt.label_length,
                    pc
                );

            if (!ok)
            {
                fprintf(
                    stderr,
                    "duplicate label: %.*s\n",
                    (int)stmt.label_length,
                    stmt.label_start
                );

                return 0;
            }

            continue;
        }


        if (
            stmt.type ==
            STATEMENT_INSTRUCTION
        )
        {
            pc += 4;
        }
    }
    return 1;
}

int assembler_second_pass(
    Parser *parser,
    const SymbolTable *symbols,
    uint32_t *output,
    size_t output_capacity,
    size_t *output_count)
{
    uint64_t pc = 0;

    size_t count = 0;


    for (;;)
    {
        Statement stmt =
            parser_next_statement(parser);

        if (stmt.type == STATEMENT_EOF)
        {
            break;
        }

        if (stmt.type == STATEMENT_INVALID)
        {
            return 0;
        }


        if (stmt.type == STATEMENT_LABEL)
        {
            continue;
        }

        if (stmt.type == STATEMENT_INSTRUCTION)
        {
            if (count >= output_capacity)
            {
                fprintf(
                    stderr,
                    "assembler error: output buffer full\n"
                );

                return 0;
            }


            uint32_t word;

            int ok =
                encode_instruction(
                    &stmt.instruction,
                    pc,
                    symbols,
                    &word
                );


            if (!ok)
            {
                fprintf(
                    stderr,
                    "assembler error: could not encode instruction at PC %llu\n",
                    (unsigned long long)pc
                );

                return 0;
            }

            output[count] = word;
            count++;
            pc += 4;
        }
    }
    if (output_count != NULL)
    {
        *output_count = count;
    }
    return 1;
}

int write_binary(
    const char *filename,
    const uint32_t *instructions,
    size_t instruction_count)
{
    FILE *file =
        fopen(filename, "wb");

    if (file == NULL)
    {
        perror("could not open output file");
        return 0;
    }

    for (
        size_t i = 0;
        i < instruction_count;
        i++)
    {
        uint32_t word = instructions[i];

        uint8_t bytes[4];

        bytes[0] =
            word & 0xFF;
        bytes[1] =
            (word >> 8) & 0xFF;
        bytes[2] =
            (word >> 16) & 0xFF;
        bytes[3] =
            (word >> 24) & 0xFF;

        size_t written =
            fwrite(
                bytes,
                1,
                4,
                file
            );


        if (written != 4)
        {
            perror("could not write instruction");
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}