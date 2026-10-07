#ifndef CPU_H
#define CPU_H

#include <stddef.h>
#include <stdint.h>

#define REG_COUNT 31

extern uint64_t reg[REG_COUNT];
extern uint64_t pc;
extern uint8_t nzcv;

void cpu_reset(void);
int cpu_load_words(const uint32_t *words,size_t count);
int cpu_step(void);
int cpu_run(void);
int cpu_finished(void);

uint32_t cpu_current_instruction(void);
uint64_t mem_read64(uint64_t address);

#endif
