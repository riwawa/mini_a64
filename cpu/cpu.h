#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define REG_COUNT 31

extern uint64_t reg[REG_COUNT];
extern uint64_t pc;
extern uint8_t nzcv;

int run_binary(const char *path);
void dump_registers(void);

#endif