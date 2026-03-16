# FORGE Toolchain Makefile
# C99 standard, simple build system

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -g -I src
RELEASE_FLAGS = -std=c99 -O2 -DNDEBUG -I src
LDFLAGS = -lm

# Optional GUI support (raylib + raygui) — vendored in vendor/raylib/
# Build with: make GUI=1
ifeq ($(GUI),1)
  CFLAGS  += -DFORGE_HAS_GUI -I vendor/raylib/include
  RELEASE_FLAGS += -DFORGE_HAS_GUI -I vendor/raylib/include
  LDFLAGS += -L vendor/raylib/lib -lraylib -lGL -lpthread -ldl -lrt -lX11
endif

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
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

release:
	$(CC) $(RELEASE_FLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

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

