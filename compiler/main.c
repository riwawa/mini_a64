#include <stdio.h>
#include <stdlib.h>
#include "parser.h"

static char *read_file(const char *path){
    FILE *file = fopen(path, "rb");

    if(!file){
        perror(path);
        return NULL;
    }

    if(fseek(file, 0, SEEK_END) != 0){
        fclose(file);
        return NULL;
    }

    long size = ftell(file);

    if(size < 0){
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *source = malloc((size_t)size+1);

    if(!source){
        fclose(file);
        return NULL;
    }

    size_t read = fread(source, 1, (size_t)size, file);
    source[read] = '\0';

    fclose(file);
    return source;
}

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "usage: %s <source>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *source = read_file(argv[1]);

    if(!source) return EXIT_FAILURE;

    Parser parser;
    parser_init(&parser, source);

    if(!parse_program(&parser)){
        free(source);
        return EXIT_FAILURE;
    }
    printf("%s", emitter_output(&parser.emitter));

    free(source);
    return EXIT_SUCCESS;
}
