"""
FORGE Language Syntax Highlighter
Provides syntax highlighting for the FORGE programming language
"""

import re
from PyQt6.QtGui import QSyntaxHighlighter, QTextCharFormat, QColor, QFont

from forge_ide.app.themes import SyntaxColors


class PlainHighlighter(QSyntaxHighlighter):
    """Syntax highlighter for FORGE language"""

    def __init__(self, parent=None, syntax_theme: SyntaxColors = None):
        super().__init__(parent)
        self.syntax_theme = syntax_theme
        self._setup_rules()

    def set_syntax_theme(self, syntax_theme: SyntaxColors):
        """Update the syntax theme and refresh highlighting"""
        self.syntax_theme = syntax_theme
        self._setup_rules()
        self.rehighlight()

    def _create_format(self, color: str, bold: bool = False, italic: bool = False) -> QTextCharFormat:
        """Create a text format with the given style"""
        fmt = QTextCharFormat()
        fmt.setForeground(QColor(color))
        if bold:
            fmt.setFontWeight(QFont.Weight.Bold)
        if italic:
            fmt.setFontItalic(True)
        return fmt

    def _setup_rules(self):
        """Set up highlighting rules based on current syntax theme"""
        self.rules = []

        if self.syntax_theme is None:
            return

        syntax = self.syntax_theme

        # Keywords - declarations / structure
        decl_keywords = r'\b(proc|record|var|const|export|import)\b'
        self.rules.append((re.compile(decl_keywords), self._create_format(syntax.keyword, bold=True)))

        # Keywords - control flow
        control_keywords = r'\b(if|elif|else|for|while|loop|break|continue|return|in|range|with|as)\b'
        self.rules.append((re.compile(control_keywords), self._create_format(syntax.keyword, bold=True)))

        # Keywords - channel / event system
        channel_keywords = r'\b(channel|emit|on)\b'
        self.rules.append((re.compile(channel_keywords), self._create_format(syntax.keyword, bold=True)))

        # Keywords - memory / optional
        mem_keywords = r'\b(free|some|is|not|and|or)\b'
        self.rules.append((re.compile(mem_keywords), self._create_format(syntax.keyword, bold=True)))

        # Types
        type_names = r'\b(int|uint|float|str|bool|void|byte|map)\b'
        self.rules.append((re.compile(type_names), self._create_format(syntax.type)))

        # Boolean / null literals
        literals = r'\b(true|false|none)\b'
        self.rules.append((re.compile(literals), self._create_format(syntax.constant)))

        # Built-in standard library functions
        builtins = (
            r'\b(print|println|input|len|str|int|uint|float|bool|'
            r'assert|exit|panic|'
            r'append|remove|contains|keys|values|'
            r'abs|min|max|floor|ceil|round|sqrt|'
            r'now|sleep)\b'
        )
        self.rules.append((re.compile(builtins), self._create_format(syntax.builtin)))

        # Procedure / function call — identifier followed by '('
        func_call = r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*(?=\()'
        self.rules.append((re.compile(func_call), self._create_format(syntax.function)))

        # Numbers — decimal, hex, binary, octal (with optional _ separators)
        numbers = r'\b(0x[0-9a-fA-F][0-9a-fA-F_]*|0b[01][01_]*|0o[0-7][0-7_]*|\d[\d_]*\.?\d*([eE][+-]?\d+)?)\b'
        self.rules.append((re.compile(numbers), self._create_format(syntax.number)))

        # Strings — double-quoted only (FORGE has no single-quote strings)
        strings = r'"[^"\\]*(\\.[^"\\]*)*"'
        self.rules.append((re.compile(strings), self._create_format(syntax.string)))

        # Arrow / type annotation operators (->)
        arrow = r'->'
        self.rules.append((re.compile(arrow), self._create_format(syntax.operator, bold=True)))

        # General operators
        operators = r'[+\-*/%=<>!&|^~:]+'
        self.rules.append((re.compile(operators), self._create_format(syntax.operator)))

        # Line comments — '#' to end of line (applied last so they override everything)
        line_comment = r'#.*$'
        self.rules.append((re.compile(line_comment), self._create_format(syntax.comment, italic=True)))

    def highlightBlock(self, text: str):
        """Apply highlighting to a block of text"""
        if self.syntax_theme is None:
            return

        self.setCurrentBlockState(0)

        # Apply all rules in order
        for pattern, fmt in self.rules:
            for match in pattern.finditer(text):
                self.setFormat(match.start(), match.end() - match.start(), fmt)



