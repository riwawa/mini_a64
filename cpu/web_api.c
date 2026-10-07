#include <stddef.h>
#include <stdint.h>
#include "cpu.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

#define MAX_INSTRUCTIONS 4096

static uint32_t program[MAX_INSTRUCTIONS];
static size_t program_count=0;

EXPORT void mini_cpu_clear_program(void){
    program_count=0;
}

EXPORT int mini_cpu_add_instruction(uint32_t word){
    if(program_count>=MAX_INSTRUCTIONS)
        return 0;

    program[program_count++]=word;
    return 1;
}

EXPORT int mini_cpu_prepare(void){
    return cpu_load_words(program,program_count);
}

EXPORT int mini_cpu_step(void){
    return cpu_step();
}

EXPORT int mini_cpu_run(void){
    return cpu_run();
}

EXPORT int mini_cpu_finished(void){
    return cpu_finished();
}

EXPORT uint32_t mini_cpu_instruction(void){
    return cpu_current_instruction();
}

EXPORT unsigned long long mini_cpu_register(int index){
    if(index<0 || index>=REG_COUNT)
        return 0;

    return reg[index];
}

EXPORT unsigned long long mini_cpu_pc(void){
    return pc;
}

EXPORT int mini_cpu_nzcv(void){
    return nzcv;
}

EXPORT unsigned long long mini_cpu_memory64(unsigned long long address){
    return mem_read64((uint64_t)address);
}
