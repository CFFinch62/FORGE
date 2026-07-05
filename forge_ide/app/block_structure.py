"""
Shared FORGE language block-structure rules.

This module is the single source of truth for "what counts as the start of
an indented block" in FORGE. It's used by:
  - editor.py's auto-indent logic (keyPressEvent)
  - editor.py's nested scope-box renderer (paintEvent)

Keeping this logic in one place means auto-indent and the visual nested
scope boxes can never quietly drift apart and disagree about where a block
starts.

FORGE is indentation-significant (Python-style INDENT/DEDENT tokens in the
lexer, see FORGE/dev_docs/DEV_SESSION_LOG.md task 1.9), so the dedent-stack
algorithm below is valid the same way it is for PLAIN. Unlike PLAIN, every
FORGE block header ends with ':' (proc, if, elif, else, while, for) -- there
is no bare-keyword block opener -- so no keyword whitelist is needed here.

This is a lightweight indentation-tracker, not a real parser. It does not
validate syntax and will happily produce scopes for malformed or mid-edit
code. That's intentional: it needs to keep working while the user is
halfway through typing and the code doesn't parse yet.
"""

from typing import List, Tuple


def line_opens_block(line: str) -> bool:
    """
    Return True if this line's content opens a new indented block, i.e. the
    following, more-indented lines belong to it.

    Every FORGE block header (proc, if, elif, else, while, for, records,
    etc.) ends with ':', so that alone is sufficient.
    """
    text = line.strip()
    if not text:
        return False
    return text.endswith(":")


def compute_scopes(text: str) -> List[Tuple[int, int, int]]:
    """
    Walk the full document text and compute nested scope ranges based on
    indentation, using line_opens_block() to decide which lines are block
    headers.

    Returns a list of (start_line, end_line, depth) tuples, 0-indexed and
    inclusive on both ends, matching QTextDocument block numbers.
    """
    lines = text.split("\n")
    scopes: List[Tuple[int, int, int]] = []
    # Stack entries: (start_line_index, header_indent, depth)
    stack: List[Tuple[int, int, int]] = []

    def last_nonblank(before_index: int, floor: int) -> int:
        idx = before_index
        while idx > floor and not lines[idx].strip():
            idx -= 1
        return idx

    for i, raw_line in enumerate(lines):
        stripped = raw_line.strip()
        if not stripped:
            continue  # blank lines never close or open a scope

        indent = len(raw_line) - len(raw_line.lstrip())

        # Close every open scope we've now dedented back to or past.
        while stack and indent <= stack[-1][1]:
            start_idx, _header_indent, depth = stack.pop()
            end_idx = last_nonblank(i - 1, start_idx)
            scopes.append((start_idx, end_idx, depth))

        if line_opens_block(stripped):
            depth = len(stack)
            stack.append((i, indent, depth))

    # Close any scopes still open at end of document.
    last_idx = last_nonblank(len(lines) - 1, 0)
    while stack:
        start_idx, _header_indent, depth = stack.pop()
        scopes.append((start_idx, last_idx, depth))

    return scopes
