#ifndef STATEMENT_H
#define STATEMENT_H

#include <stddef.h>
#include "instruction.h"


typedef enum
{
    STATEMENT_INSTRUCTION,
    STATEMENT_LABEL,
    STATEMENT_EOF,
    STATEMENT_INVALID

} StatementType;


typedef struct
{
    StatementType type;
    Instruction instruction;

    const char *label_start;
    size_t label_length;
} Statement;


#endif