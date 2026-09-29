CC = gcc

CPU_DIR = cpu
TEST_DIR = tests

CPU_SRC = $(CPU_DIR)/cpu.c
CPU_MAIN = $(CPU_DIR)/main.c
TEST_SRC = $(TEST_DIR)/test_cpu.c

CPU_BIN = mini_a64_cpu
TEST_BIN = test_cpu


.PHONY: all cpu test clean rebuild


all: cpu test


cpu:
	$(CC) $(CPU_MAIN) $(CPU_SRC) -o $(CPU_BIN)


test:
	$(CC) $(TEST_SRC) $(CPU_SRC) -o $(TEST_BIN)
	./$(TEST_BIN)


clean:
	rm -f $(CPU_BIN)
	rm -f $(TEST_BIN)
	rm -f tests/bin/*.bin


rebuild: clean all