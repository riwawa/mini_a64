#include <stdio.h>
#include <stdarg.h>
#include "emitter.h"

void emitter_init(Emitter *emitter){
    emitter->length = 0;
    emitter->buffer[0] = '\0';
}

int emitter_write(Emitter *emitter, const char *format, ...){
    if(emitter->length >= EMITTER_CAPACITY) return 0;

    va_list args;
    va_start(args, format);

    int written = vsnprintf(
        emitter->buffer+emitter->length,
        EMITTER_CAPACITY-emitter->length,
        format,
        args
    );

    va_end(args);

    if(written < 0) return 0;

    if((size_t)written >= EMITTER_CAPACITY-emitter->length)
        return 0;

    emitter->length += (size_t)written;
    return 1;
}

const char *emitter_output(Emitter *emitter){
    return emitter->buffer;
}
