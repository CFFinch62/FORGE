/*
 * Demo: AST Pretty-Printer
 *
 * This program demonstrates the ast_print() function by parsing a
 * sample FORGE program and printing its AST.
 */

#include <stdio.h>
#include "util/memory.h"
#include "util/strtable.h"
#include "lexer/lexer.h"
#include "parser/ast.h"
#include "parser/parser.h"

int main(void) {
    /* Sample FORGE program */
    const char* source =
        "record Point:\n"
        "    x: int\n"
        "    y: int\n"
        "\n"
        "proc main() -> void:\n"
        "    var x = 10\n"
        "    var y = 20\n"
        "    var sum = x + y * 2\n"
        "\n"
        "    if sum > 50:\n"
        "        print(\"large\")\n"
        "    elif sum > 25:\n"
        "        print(\"medium\")\n"
        "    else:\n"
        "        print(\"small\")\n"
        "\n"
        "    for i in range(0, 10):\n"
        "        sum += i\n"
        "\n"
        "    return\n";

    printf("=== FORGE Source ===\n\n");
    printf("%s\n", source);
    printf("=== AST Pretty-Print ===\n\n");

    /* Create allocator and string table */
    forge_arena_t* arena = arena_create(8192);
    forge_strtable_t* strtable = strtable_create();

    /* Tokenize */
    int source_len = 0;
    for (const char* s = source; *s; s++) source_len++;
    forge_lexer_t* lexer = lexer_create(source, source_len, "<demo>", strtable);
    lexer_tokenize(lexer);

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                            arena, strtable, "<demo>");
    forge_node_t* program = parser_parse(parser);

    if (parser_had_error(parser)) {
        printf("Parse errors occurred.\n");
    } else if (program) {
        ast_print(program, 0);
    }

    /* Cleanup */
    parser_destroy(parser);
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);

    return 0;
}

