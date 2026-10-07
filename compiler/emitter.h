#ifndef EMITTER_H
#define EMITTER_H

#include <stddef.h>

#define EMITTER_CAPACITY 8192

typedef struct{
    char buffer[EMITTER_CAPACITY];
    size_t length;
} Emitter;

void emitter_init(Emitter *emitter);
int emitter_write(Emitter *emitter, const char *format, ...);
const char *emitter_output(Emitter *emitter);

#endif
