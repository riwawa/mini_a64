CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

CPU_DIR = cpu
ASM_DIR = assembler
COMPILER_DIR = compiler
TEST_DIR = tests

CPU_BIN = mini_a64_cpu
ASM_BIN = $(ASM_DIR)/mini_a64_as
COMPILER_BIN = $(COMPILER_DIR)/mini_a64_cc
TEST_BIN = test_cpu

CPU_SRC = $(CPU_DIR)/cpu.c
CPU_MAIN = $(CPU_DIR)/main.c

COMPILER_SRC = \
	$(COMPILER_DIR)/main.c \
	$(COMPILER_DIR)/parser.c \
	$(COMPILER_DIR)/lexer.c \
	$(COMPILER_DIR)/symbol_table.c \
	$(COMPILER_DIR)/emitter.c

WEB_COMPILER_SRC = \
	$(COMPILER_DIR)/parser.c \
	$(COMPILER_DIR)/lexer.c \
	$(COMPILER_DIR)/symbol_table.c \
	$(COMPILER_DIR)/emitter.c \
	$(COMPILER_DIR)/web_api.c
	
SOURCE ?= $(COMPILER_DIR)/test.mini
ASM_OUT = $(SOURCE:.mini=.s)
BIN_OUT = $(SOURCE:.mini=.bin)

.PHONY: all cpu compiler test run clean rebuild

all: cpu compiler

cpu:
	$(CC) $(CFLAGS) $(CPU_MAIN) $(CPU_SRC) -o $(CPU_BIN)

compiler:
	$(CC) $(CFLAGS) $(COMPILER_SRC) -o $(COMPILER_BIN)

test:
	$(CC) $(CFLAGS) $(TEST_DIR)/test_cpu.c $(CPU_SRC) -o $(TEST_BIN)
	./$(TEST_BIN)

run: cpu compiler
	$(COMPILER_BIN) $(SOURCE) > $(ASM_OUT)
	$(ASM_BIN) $(ASM_OUT) $(BIN_OUT)
	./$(CPU_BIN) $(BIN_OUT)

clean:
	rm -f $(CPU_BIN)
	rm -f $(TEST_BIN)
	rm -f $(COMPILER_BIN)
	rm -f compiler/*.s
	rm -f compiler/*.bin
	rm -f programs/*.bin
	rm -f tests/bin/*.bin

rebuild: clean all