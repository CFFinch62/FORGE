"""
Help Viewer for FORGE IDE
Displays FORGE language quick reference and documentation
"""

import re
from pathlib import Path
from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QTextBrowser,
    QLineEdit, QPushButton, QLabel
)
from PyQt6.QtCore import Qt

from forge_ide.app.themes import Theme
from forge_ide.app.utils import get_resource_path


class HelpViewer(QDialog):
    """Help/documentation viewer dialog"""

    def __init__(self, parent=None, theme: Theme = None):
        super().__init__(parent)
        self.theme = theme
        self.setWindowTitle("FORGE Quick Reference")
        self.setMinimumSize(700, 500)
        self._setup_ui()
        self._load_content()

    def _setup_ui(self):
        """Set up the help viewer UI"""
        layout = QVBoxLayout(self)

        # Search bar
        search_layout = QHBoxLayout()
        search_label = QLabel("Search:")
        search_layout.addWidget(search_label)

        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("Search documentation...")
        self.search_input.textChanged.connect(self._on_search)
        search_layout.addWidget(self.search_input)

        layout.addLayout(search_layout)

        # Content browser
        self.browser = QTextBrowser()
        self.browser.setOpenExternalLinks(False)
        layout.addWidget(self.browser)

        # Close button
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.close)
        btn_layout.addWidget(close_btn)
        layout.addLayout(btn_layout)

    def _load_content(self):
        """Load the quick reference markdown and convert to HTML"""
        content = None

        # Primary: use robust resource path resolution (works frozen + dev)
        ref_path = get_resource_path("user_docs/quick_reference.md")
        if Path(ref_path).exists():
            content = Path(ref_path).read_text(encoding='utf-8')

        # Fallback: look relative to this source file (dev mode only)
        if content is None:
            dev_path = Path(__file__).parent.parent.parent / "user_docs" / "quick_reference.md"
            if dev_path.exists():
                content = dev_path.read_text(encoding='utf-8')

        if content is None:
            content = f"# FORGE Quick Reference\n\nDocumentation file not found at:\n`{ref_path}`"

        html = self._markdown_to_html(content)
        self._full_html = html
        self.browser.setHtml(html)

    def _markdown_to_html(self, md: str) -> str:
        """Simple markdown to HTML conversion"""
        lines = md.split('\n')
        html_lines = []
        in_code_block = False
        in_list = False

        for line in lines:
            # Code blocks
            if line.strip().startswith('```'):
                if in_code_block:
                    html_lines.append('</pre>')
                    in_code_block = False
                else:
                    html_lines.append('<pre style="background-color: #2a2a3c; color: #cdd6f4; padding: 12px; border-radius: 6px; font-family: monospace; font-size: 12px; overflow-x: auto;">')
                    in_code_block = True
                continue

            if in_code_block:
                # Escape HTML in code blocks
                line = line.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                html_lines.append(line)
                continue

            # Close list if needed
            if in_list and not line.strip().startswith(('-', '*', '1.', '2.', '3.', '4.', '5.', '6.', '7.', '8.', '9.')):
                if not line.strip().startswith(' '):
                    html_lines.append('</ul>')
                    in_list = False

            # Headings
            if line.startswith('### '):
                html_lines.append(f'<h3>{self._inline_format(line[4:])}</h3>')
            elif line.startswith('## '):
                html_lines.append(f'<h2>{self._inline_format(line[3:])}</h2>')
            elif line.startswith('# '):
                html_lines.append(f'<h1>{self._inline_format(line[2:])}</h1>')
            # Horizontal rule
            elif line.strip() == '---':
                html_lines.append('<hr>')
            # List items
            elif line.strip().startswith('- ') or line.strip().startswith('* '):
                if not in_list:
                    html_lines.append('<ul>')
                    in_list = True
                item = line.strip()[2:]
                item = self._inline_format(item)
                html_lines.append(f'<li>{item}</li>')
            # Numbered list
            elif re.match(r'^\d+\.\s', line.strip()):
                if not in_list:
                    html_lines.append('<ul>')
                    in_list = True
                item = re.sub(r'^\d+\.\s', '', line.strip())
                item = self._inline_format(item)
                html_lines.append(f'<li>{item}</li>')
            # Empty line
            elif not line.strip():
                html_lines.append('<br>')
            # Regular paragraph
            else:
                formatted = self._inline_format(line)
                html_lines.append(f'<p>{formatted}</p>')

        if in_list:
            html_lines.append('</ul>')
        if in_code_block:
            html_lines.append('</pre>')

        return '\n'.join(html_lines)

    def _inline_format(self, text: str) -> str:
        """Apply inline markdown formatting"""
        # Split on inline code spans to handle them separately
        parts = re.split(r'(`[^`]+`)', text)
        result = []
        for part in parts:
            if part.startswith('`') and part.endswith('`') and len(part) > 2:
                # Inline code: escape HTML inside, then wrap
                code = part[1:-1]
                code = code.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                result.append(
                    f'<code style="background-color: #2a2a3c; padding: 2px 6px;'
                    f' border-radius: 3px; font-family: monospace;">{code}</code>'
                )
            else:
                # Regular text: escape HTML, then apply bold/italic
                part = part.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                part = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', part)
                part = re.sub(r'\*(.+?)\*', r'<i>\1</i>', part)
                result.append(part)
        return ''.join(result)

    def _on_search(self, text: str):
        """Search within the help content"""
        if not text:
            self.browser.setHtml(self._full_html)
            return

        # Use QTextBrowser's built-in find
        if not self.browser.find(text):
            # If not found, wrap around
            cursor = self.browser.textCursor()
            cursor.movePosition(cursor.MoveOperation.Start)
            self.browser.setTextCursor(cursor)
            self.browser.find(text)

    def apply_theme(self, theme: Theme):
        """Apply theme to the help viewer"""
        self.theme = theme
        self.setStyleSheet(f"""
            QDialog {{
                background-color: {theme.background};
                color: {theme.foreground};
            }}
            QTextBrowser {{
                background-color: {theme.editor_background};
                color: {theme.editor_foreground};
                border: 1px solid {theme.panel_border};
                border-radius: 6px;
                padding: 12px;
                font-size: 13px;
            }}
            QLineEdit {{
                background-color: {theme.input_background};
                color: {theme.foreground};
                border: 1px solid {theme.input_border};
                border-radius: 4px;
                padding: 6px 10px;
            }}
            QLineEdit:focus {{
                border-color: {theme.input_focus_border};
            }}
            QPushButton {{
                background-color: {theme.button_background};
                color: {theme.button_foreground};
                border: none;
                border-radius: 4px;
                padding: 6px 16px;
            }}
            QPushButton:hover {{
                background-color: {theme.button_hover};
            }}
            QLabel {{
                color: {theme.foreground};
            }}
        """)
