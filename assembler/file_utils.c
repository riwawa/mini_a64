#include <stdio.h>
#include <stdlib.h>

#include "file_utils.h"


char *read_text_file(
    const char *filename)
{
    FILE *file =
        fopen(filename, "rb");

    if (file == NULL)
    {
        perror("could not open source file");
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    long size =
        ftell(file);

    if (size < 0)
    {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *buffer =
        malloc((size_t)size + 1);

    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }


    size_t read =
        fread(
            buffer,
            1,
            (size_t)size,
            file
        );


    if (read != (size_t)size)
    {
        free(buffer);
        fclose(file);
        return NULL;
    }


    /*
     * Transformamos os bytes lidos
     * em string C.
     */
    buffer[size] = '\0';


    fclose(file);

    return buffer;
}
