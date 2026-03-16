#!/bin/bash
# FORGE Validated Test Runner
# Runs .fg test files and compares output against .expected files
# Usage: ./tests/run_tests.sh [directory...] [--update] [--verbose] [--compile] [--target c|llvm]
#   directory:  test directory to run (default: all test dirs)
#   --update:   regenerate .expected files from current output
#   --verbose:  show output diff on failure
#   --compile:  compile tests with forge build and run the binary (default: interpret)
#   --target:   compilation backend: c (default) or llvm (requires --compile)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FORGE="$PROJECT_DIR/forge"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Parse args
UPDATE_MODE=0
VERBOSE=0
COMPILE_MODE=0
COMPILE_TARGET="c"
TARGET_DIRS=()
i=1
while [ $i -le $# ]; do
    arg="${!i}"
    case "$arg" in
        --update)  UPDATE_MODE=1 ;;
        --verbose) VERBOSE=1 ;;
        --compile) COMPILE_MODE=1 ;;
        --target)
            i=$((i + 1))
            COMPILE_TARGET="${!i}"
            COMPILE_MODE=1
            ;;
        *)         TARGET_DIRS+=("$arg") ;;
    esac
    i=$((i + 1))
done

# Counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0
BUILD_FAIL=0
ERRORS=""

# Test timeout in seconds
TIMEOUT=30

# Mode label for display
if [ "$COMPILE_MODE" -eq 1 ]; then
    MODE_LABEL="compiled (${COMPILE_TARGET})"
else
    MODE_LABEL="interpreted"
fi

# Execute a test: either interpret or compile+run
# Sets EXEC_OUTPUT and EXEC_EXIT
exec_test() {
    local test_file="$1"
    EXEC_EXIT=0
    if [ "$COMPILE_MODE" -eq 1 ]; then
        local tmp_bin="/tmp/forge_test_$$"
        # Build (suppress forge's status output)
        if ! timeout "$TIMEOUT" "$FORGE" build "$test_file" -o "$tmp_bin" --target "$COMPILE_TARGET" -q 2>/dev/null; then
            EXEC_OUTPUT="BUILD_FAILED"
            EXEC_EXIT=99
            return
        fi
        # Run the compiled binary
        EXEC_OUTPUT=$(timeout "$TIMEOUT" "$tmp_bin" 2>&1) || EXEC_EXIT=$?
        rm -f "$tmp_bin"
    else
        EXEC_OUTPUT=$(timeout "$TIMEOUT" "$FORGE" run "$test_file" 2>&1) || EXEC_EXIT=$?
    fi
    # Normalize absolute project paths to relative
    EXEC_OUTPUT=$(echo "$EXEC_OUTPUT" | sed "s|${PROJECT_DIR}/||g")
}

run_test() {
    local test_file="$1"
    local expected_file="${test_file%.fg}.expected"
    local test_name
    test_name="$(basename "$test_file" .fg)"
    local dir_name
    dir_name="$(basename "$(dirname "$test_file")")"
    local label="${dir_name}/${test_name}"

    TOTAL=$((TOTAL + 1))

    # Update mode: capture output and write .expected (always via interpreter)
    if [ "$UPDATE_MODE" -eq 1 ]; then
        local output
        output=$(timeout "$TIMEOUT" "$FORGE" run "$test_file" 2>&1) || true
        output=$(echo "$output" | sed "s|${PROJECT_DIR}/||g")
        echo "$output" > "$expected_file"
        echo -e "  ${CYAN}UPDATED${NC}  $label"
        PASSED=$((PASSED + 1))
        return
    fi

    # Check for .expected or .expected_lines file
    local pattern_file="${test_file%.fg}.expected_lines"
    local use_pattern=0
    if [ -f "$pattern_file" ]; then
        use_pattern=1
    elif [ ! -f "$expected_file" ]; then
        echo -e "  ${YELLOW}SKIP${NC}     $label  (no .expected file)"
        SKIPPED=$((SKIPPED + 1))
        return
    fi

    # Run the test
    exec_test "$test_file"
    local actual="$EXEC_OUTPUT"
    local exit_code="$EXEC_EXIT"

    # Build failure in compile mode
    if [ "$exit_code" -eq 99 ]; then
        echo -e "  ${RED}BUILDFAIL${NC} $label"
        BUILD_FAIL=$((BUILD_FAIL + 1))
        FAILED=$((FAILED + 1))
        ERRORS="${ERRORS}\n  BUILDFAIL: ${label}"
        return
    fi

    if [ "$exit_code" -eq 124 ]; then
        echo -e "  ${RED}TIMEOUT${NC}  $label  (exceeded ${TIMEOUT}s)"
        FAILED=$((FAILED + 1))
        ERRORS="${ERRORS}\n  TIMEOUT: ${label}"
        return
    fi

    # Pattern mode: each line in .expected_lines must appear in output
    if [ "$use_pattern" -eq 1 ]; then
        local missing=""
        while IFS= read -r line; do
            [ -z "$line" ] && continue
            if ! echo "$actual" | grep -qF "$line"; then
                missing="${missing}\n    MISSING: ${line}"
            fi
        done < "$pattern_file"

        if [ -z "$missing" ]; then
            echo -e "  ${GREEN}PASS${NC}     $label"
            PASSED=$((PASSED + 1))
        else
            echo -e "  ${RED}FAIL${NC}     $label"
            FAILED=$((FAILED + 1))
            ERRORS="${ERRORS}\n  FAIL: ${label}"
            if [ "$VERBOSE" -eq 1 ]; then
                echo -e "$missing"
            fi
        fi
        return
    fi

    # Exact comparison mode
    local expected
    expected=$(cat "$expected_file")

    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC}     $label"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}     $label"
        FAILED=$((FAILED + 1))
        ERRORS="${ERRORS}\n  FAIL: ${label}"
        if [ "$VERBOSE" -eq 1 ]; then
            echo "    --- expected ---"
            echo "$expected" | head -20
            echo "    --- actual ---"
            echo "$actual" | head -20
            echo "    ---"
        fi
    fi
}

run_error_test() {
    local test_file="$1"
    local expected_file="${test_file%.fg}.expected"
    local test_name
    test_name="$(basename "$test_file" .fg)"
    local label="error/${test_name}"

    # Error tests only make sense for the interpreter — compiled binaries
    # produce different runtime error messages. Skip in compile mode.
    if [ "$COMPILE_MODE" -eq 1 ]; then
        TOTAL=$((TOTAL + 1))
        echo -e "  ${YELLOW}SKIP${NC}     $label  (error tests use interpreter only)"
        SKIPPED=$((SKIPPED + 1))
        return
    fi

    TOTAL=$((TOTAL + 1))

    if [ "$UPDATE_MODE" -eq 1 ]; then
        local output
        output=$(timeout "$TIMEOUT" "$FORGE" run "$test_file" 2>&1) || true
        output=$(echo "$output" | sed "s|${PROJECT_DIR}/||g")
        echo "$output" > "$expected_file"
        echo -e "  ${CYAN}UPDATED${NC}  $label"
        PASSED=$((PASSED + 1))
        return
    fi

    if [ ! -f "$expected_file" ]; then
        echo -e "  ${YELLOW}SKIP${NC}     $label  (no .expected file)"
        SKIPPED=$((SKIPPED + 1))
        return
    fi

    local actual
    actual=$(timeout "$TIMEOUT" "$FORGE" run "$test_file" 2>&1) || true
    actual=$(echo "$actual" | sed "s|${PROJECT_DIR}/||g")
    local expected
    expected=$(cat "$expected_file")

    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC}     $label"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}     $label"
        FAILED=$((FAILED + 1))
        ERRORS="${ERRORS}\n  FAIL: ${label}"
        if [ "$VERBOSE" -eq 1 ]; then
            echo "    --- expected ---"
            echo "$expected" | head -10
            echo "    --- actual ---"
            echo "$actual" | head -10
            echo "    ---"
        fi
    fi
}

# ── Main ──────────────────────────────────────────────────────────────────────

# Check forge binary exists
if [ ! -x "$FORGE" ]; then
    echo -e "${RED}Error: forge binary not found at $FORGE${NC}"
    echo "Run 'make' first to build."
    exit 1
fi

echo -e "${BOLD}=== FORGE Validated Test Suite (${MODE_LABEL}) ===${NC}"
echo ""

# Determine which directories to test
TEST_DIRS=()
if [ ${#TARGET_DIRS[@]} -gt 0 ]; then
    for d in "${TARGET_DIRS[@]}"; do
        TEST_DIRS+=("$SCRIPT_DIR/$d")
    done
else
    # Run all test directories in order
    for d in forge integration regression error; do
        if [ -d "$SCRIPT_DIR/$d" ]; then
            TEST_DIRS+=("$SCRIPT_DIR/$d")
        fi
    done
fi

for dir in "${TEST_DIRS[@]}"; do
    dir_name="$(basename "$dir")"
    echo -e "${BOLD}── $dir_name ──${NC}"

    if [ "$dir_name" = "error" ]; then
        for f in "$dir"/*.fg; do
            [ -f "$f" ] && run_error_test "$f"
        done
    else
        for f in "$dir"/*.fg; do
            [ -f "$f" ] && run_test "$f"
        done
    fi
    echo ""
done



# Summary
echo -e "${BOLD}═══════════════════════════════════${NC}"
echo -e "  Mode:    ${MODE_LABEL}"
echo -e "  Total:   $TOTAL"
echo -e "  ${GREEN}Passed:  $PASSED${NC}"
if [ "$FAILED" -gt 0 ]; then
    echo -e "  ${RED}Failed:  $FAILED${NC}"
fi
if [ "$BUILD_FAIL" -gt 0 ]; then
    echo -e "  ${RED}Build failures: $BUILD_FAIL${NC}"
fi
if [ "$SKIPPED" -gt 0 ]; then
    echo -e "  ${YELLOW}Skipped: $SKIPPED${NC}"
fi
echo -e "${BOLD}═══════════════════════════════════${NC}"

if [ "$FAILED" -gt 0 ]; then
    echo -e "\n${RED}Failures:${NC}"
    echo -e "$ERRORS"
    exit 1
fi

