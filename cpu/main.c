#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(
            stderr,
            "usage: %s program.bin\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }


    if (!run_binary(argv[1]))
    {
        return EXIT_FAILURE;
    }


    dump_registers();


    return EXIT_SUCCESS;
}