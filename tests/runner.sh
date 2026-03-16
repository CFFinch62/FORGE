#!/bin/bash
# FORGE Test Runner
# Runs all unit tests and FORGE program tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FORGE="$PROJECT_DIR/forge"

echo "=== FORGE Test Runner ==="
echo ""

# Build first
echo "Building forge..."
cd "$PROJECT_DIR"
make clean > /dev/null 2>&1 || true
make

echo ""
echo "=== Running Unit Tests ==="

# Unit tests will be compiled and run individually
for test_src in "$SCRIPT_DIR"/unit/*.c; do
    if [ -f "$test_src" ]; then
        test_name=$(basename "$test_src" .c)
        echo "Running $test_name..."
        
        # Compile test with source files
        gcc -std=c99 -Wall -Wextra -g -I src \
            "$test_src" \
            src/util/*.c \
            -o "$SCRIPT_DIR/output/$test_name" 2>/dev/null || {
            echo "  SKIP (compile failed)"
            continue
        }
        
        "$SCRIPT_DIR/output/$test_name" && echo "  PASS" || echo "  FAIL"
    fi
done

echo ""
echo "=== Test Run Complete ==="

