# FORGE Toolchain Makefile
# C99 standard, simple build system

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -g -I src
RELEASE_FLAGS = -std=c99 -O2 -DNDEBUG -I src

# Source files - all subdirectories
SRC_UTIL      = $(wildcard src/util/*.c)
SRC_LEXER     = $(wildcard src/lexer/*.c)
SRC_PARSER    = $(wildcard src/parser/*.c)
SRC_INTERP    = $(wildcard src/interp/*.c)
SRC_TYPECHECK = $(wildcard src/typecheck/*.c)
SRC_EMIT_C    = $(wildcard src/emit_c/*.c)
SRC_EMIT_LLVM = $(wildcard src/emit_llvm/*.c)
SRC_CLI       = $(wildcard src/cli/*.c)
SRC_RUNTIME   = $(wildcard runtime/*.c)

# Combine all sources (including runtime for interpreter serial/buffer support)
SRC = $(SRC_UTIL) $(SRC_LEXER) $(SRC_PARSER) $(SRC_INTERP) $(SRC_TYPECHECK) $(SRC_EMIT_C) $(SRC_EMIT_LLVM) $(SRC_CLI) $(SRC_RUNTIME)
OBJ = $(SRC:.c=.o)
TARGET = forge

.PHONY: all clean test release

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

release:
	$(CC) $(RELEASE_FLAGS) -o $(TARGET) $(SRC)

test:
	@echo "Running tests..."
	@bash tests/runner.sh

test-lexer: $(TARGET)
	@echo "Running lexer tests..."
	@for f in tests/forge/01_lexer/*.fg; do \
		echo "Testing $$f"; \
		./$(TARGET) lex "$$f"; \
	done

clean:
	rm -f $(OBJ) $(TARGET)
	rm -rf forge-build/

# Development helpers
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) $(ARGS)

debug: CFLAGS += -DDEBUG
debug: $(TARGET)

