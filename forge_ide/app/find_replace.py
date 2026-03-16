"""
Find/Replace Widget for FORGE IDE
Provides search and replace functionality within the code editor
"""

import re
from PyQt6.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QLineEdit, QPushButton,
    QCheckBox, QLabel, QTextEdit
)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QTextCursor, QTextCharFormat, QColor, QKeySequence

from forge_ide.app.themes import Theme


class FindReplaceWidget(QWidget):
    """Find and replace bar widget"""

    closed = pyqtSignal()

    def __init__(self, parent=None, theme: Theme = None):
        super().__init__(parent)
        self.theme = theme
        self._editor = None
        self._matches = []
        self._current_match = -1
        self._highlight_format = QTextCharFormat()
        self._highlight_format.setBackground(QColor("#f9e2af"))
        self._highlight_format.setForeground(QColor("#1e1e2e"))
        self._current_format = QTextCharFormat()
        self._current_format.setBackground(QColor("#fab387"))
        self._current_format.setForeground(QColor("#1e1e2e"))
        self._setup_ui()
        self.setVisible(False)

    def _setup_ui(self):
        """Set up the find/replace UI"""
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(8, 4, 8, 4)
        main_layout.setSpacing(4)

        # Find row
        find_row = QHBoxLayout()
        find_row.setSpacing(4)

        find_label = QLabel("Find:")
        find_label.setFixedWidth(55)
        find_row.addWidget(find_label)

        self.find_input = QLineEdit()
        self.find_input.setPlaceholderText("Search...")
        self.find_input.textChanged.connect(self._on_find_text_changed)
        self.find_input.returnPressed.connect(self.find_next)
        find_row.addWidget(self.find_input)

        self.match_label = QLabel("")
        self.match_label.setFixedWidth(70)
        self.match_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        find_row.addWidget(self.match_label)

        self.prev_btn = QPushButton("<")
        self.prev_btn.setFixedSize(28, 28)
        self.prev_btn.setToolTip("Previous match (Shift+Enter)")
        self.prev_btn.clicked.connect(self.find_previous)
        find_row.addWidget(self.prev_btn)

        self.next_btn = QPushButton(">")
        self.next_btn.setFixedSize(28, 28)
        self.next_btn.setToolTip("Next match (Enter)")
        self.next_btn.clicked.connect(self.find_next)
        find_row.addWidget(self.next_btn)

        self.case_check = QCheckBox("Aa")
        self.case_check.setToolTip("Case sensitive")
        self.case_check.toggled.connect(self._on_find_text_changed)
        find_row.addWidget(self.case_check)

        self.word_check = QCheckBox("W")
        self.word_check.setToolTip("Whole word")
        self.word_check.toggled.connect(self._on_find_text_changed)
        find_row.addWidget(self.word_check)

        self.regex_check = QCheckBox(".*")
        self.regex_check.setToolTip("Regular expression")
        self.regex_check.toggled.connect(self._on_find_text_changed)
        find_row.addWidget(self.regex_check)

        close_btn = QPushButton("x")
        close_btn.setFixedSize(28, 28)
        close_btn.setToolTip("Close (Escape)")
        close_btn.clicked.connect(self.hide_widget)
        find_row.addWidget(close_btn)

        main_layout.addLayout(find_row)

        # Replace row
        self.replace_row_widget = QWidget()
        replace_row = QHBoxLayout(self.replace_row_widget)
        replace_row.setContentsMargins(0, 0, 0, 0)
        replace_row.setSpacing(4)

        replace_label = QLabel("Replace:")
        replace_label.setFixedWidth(55)
        replace_row.addWidget(replace_label)

        self.replace_input = QLineEdit()
        self.replace_input.setPlaceholderText("Replace with...")
        replace_row.addWidget(self.replace_input)

        self.replace_btn = QPushButton("Replace")
        self.replace_btn.setFixedWidth(60)
        self.replace_btn.clicked.connect(self.replace_current)
        replace_row.addWidget(self.replace_btn)

        self.replace_all_btn = QPushButton("All")
        self.replace_all_btn.setFixedWidth(40)
        self.replace_all_btn.clicked.connect(self.replace_all)
        replace_row.addWidget(self.replace_all_btn)

        main_layout.addWidget(self.replace_row_widget)

    def set_editor(self, editor):
        """Set the target editor for find/replace operations"""
        self._editor = editor
        self._clear_highlights()
        self._matches = []
        self._current_match = -1
        self._update_match_label()

    def show_find(self):
        """Show the find bar (without replace)"""
        self.replace_row_widget.setVisible(False)
        self.setVisible(True)
        self.find_input.setFocus()
        self.find_input.selectAll()

    def show_find_replace(self):
        """Show the find and replace bar"""
        self.replace_row_widget.setVisible(True)
        self.setVisible(True)
        self.find_input.setFocus()
        self.find_input.selectAll()

    def hide_widget(self):
        """Hide the find/replace bar"""
        self._clear_highlights()
        self.setVisible(False)
        self.closed.emit()
        if self._editor:
            self._editor.setFocus()

    def _on_find_text_changed(self, *args):
        """Handle search text changes - update highlights"""
        self._find_all()

    def _build_pattern(self, text: str):
        """Build regex pattern from search text and options"""
        if not text:
            return None

        if self.regex_check.isChecked():
            try:
                pattern = text
            except re.error:
                return None
        else:
            pattern = re.escape(text)

        if self.word_check.isChecked():
            pattern = r'\b' + pattern + r'\b'

        flags = 0 if self.case_check.isChecked() else re.IGNORECASE
        try:
            return re.compile(pattern, flags)
        except re.error:
            return None

    def _find_all(self):
        """Find all matches and highlight them"""
        self._clear_highlights()
        self._matches = []
        self._current_match = -1

        if not self._editor:
            self._update_match_label()
            return

        text = self.find_input.text()
        if not text:
            self._update_match_label()
            return

        pattern = self._build_pattern(text)
        if not pattern:
            self._update_match_label()
            return

        doc_text = self._editor.toPlainText()
        for match in pattern.finditer(doc_text):
            self._matches.append((match.start(), match.end()))

        # Highlight all matches
        self._apply_highlights()

        # Jump to first match near cursor
        if self._matches:
            cursor_pos = self._editor.textCursor().position()
            self._current_match = 0
            for i, (start, end) in enumerate(self._matches):
                if start >= cursor_pos:
                    self._current_match = i
                    break
            self._go_to_match(self._current_match)

        self._update_match_label()

    def _apply_highlights(self):
        """Apply highlight formatting to all matches"""
        if not self._editor or not self._matches:
            return

        # Use extra selections for highlighting (non-destructive)
        selections = []
        for i, (start, end) in enumerate(self._matches):
            selection = QTextEdit.ExtraSelection()
            fmt = QTextCharFormat(self._current_format if i == self._current_match else self._highlight_format)
            selection.format = fmt
            cursor = self._editor.textCursor()
            cursor.setPosition(start)
            cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
            selection.cursor = cursor
            selections.append(selection)

        self._editor.setExtraSelections(selections)

    def _clear_highlights(self):
        """Clear all search highlights"""
        if self._editor:
            self._editor.setExtraSelections([])

    def _go_to_match(self, index: int):
        """Navigate to a specific match"""
        if not self._editor or not self._matches or index < 0:
            return

        start, end = self._matches[index]
        cursor = self._editor.textCursor()
        cursor.setPosition(start)
        cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
        self._editor.setTextCursor(cursor)
        self._editor.centerCursor()

        # Update highlights to show current match differently
        self._current_match = index
        self._apply_highlights()
        self._update_match_label()

    def find_next(self):
        """Go to the next match"""
        if not self._matches:
            self._find_all()
            return

        if self._matches:
            self._current_match = (self._current_match + 1) % len(self._matches)
            self._go_to_match(self._current_match)

    def find_previous(self):
        """Go to the previous match"""
        if not self._matches:
            self._find_all()
            return

        if self._matches:
            self._current_match = (self._current_match - 1) % len(self._matches)
            self._go_to_match(self._current_match)

    def replace_current(self):
        """Replace the current match"""
        if not self._editor or not self._matches or self._current_match < 0:
            return

        start, end = self._matches[self._current_match]
        replacement = self.replace_input.text()

        cursor = self._editor.textCursor()
        cursor.setPosition(start)
        cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
        cursor.insertText(replacement)

        # Re-find all matches after replacement
        self._find_all()

    def replace_all(self):
        """Replace all matches"""
        if not self._editor or not self._matches:
            return

        replacement = self.replace_input.text()
        cursor = self._editor.textCursor()
        cursor.beginEditBlock()

        # Replace from end to start to preserve positions
        for start, end in reversed(self._matches):
            cursor.setPosition(start)
            cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
            cursor.insertText(replacement)

        cursor.endEditBlock()

        # Re-find (should find nothing if not replacing with same pattern)
        self._find_all()

    def _update_match_label(self):
        """Update the match count label"""
        if not self._matches:
            if self.find_input.text():
                self.match_label.setText("No results")
            else:
                self.match_label.setText("")
        else:
            self.match_label.setText(f"{self._current_match + 1}/{len(self._matches)}")

    def keyPressEvent(self, event):
        """Handle key events"""
        if event.key() == Qt.Key.Key_Escape:
            self.hide_widget()
        elif event.key() == Qt.Key.Key_Return and event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
            self.find_previous()
        else:
            super().keyPressEvent(event)

    def apply_theme(self, theme: Theme):
        """Apply theme styling"""
        self.theme = theme

        # Update highlight colors based on theme
        if theme.is_dark:
            self._highlight_format.setBackground(QColor("#f9e2af"))
            self._highlight_format.setForeground(QColor("#1e1e2e"))
            self._current_format.setBackground(QColor("#fab387"))
            self._current_format.setForeground(QColor("#1e1e2e"))
        else:
            self._highlight_format.setBackground(QColor("#f9e2af"))
            self._highlight_format.setForeground(QColor("#4c4f69"))
            self._current_format.setBackground(QColor("#fe640b"))
            self._current_format.setForeground(QColor("#eff1f5"))

        self.setStyleSheet(f"""
            QWidget {{
                background-color: {theme.panel_background};
                color: {theme.foreground};
            }}
            QLineEdit {{
                background-color: {theme.input_background};
                color: {theme.foreground};
                border: 1px solid {theme.input_border};
                border-radius: 4px;
                padding: 4px 8px;
            }}
            QLineEdit:focus {{
                border-color: {theme.input_focus_border};
            }}
            QPushButton {{
                background-color: {theme.button_background};
                color: {theme.button_foreground};
                border: none;
                border-radius: 4px;
                padding: 4px 8px;
                font-size: 12px;
            }}
            QPushButton:hover {{
                background-color: {theme.button_hover};
            }}
            QCheckBox {{
                color: {theme.foreground};
                spacing: 4px;
            }}
            QLabel {{
                color: {theme.foreground};
                font-size: 12px;
            }}
        """)

        # Re-apply highlights with new colors if active
        if self._matches:
            self._apply_highlights()
