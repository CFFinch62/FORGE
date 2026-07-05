"""
Code Editor Widget for FORGE IDE
Provides a code editor with line numbers and syntax highlighting
"""

from PyQt6.QtWidgets import QPlainTextEdit, QWidget, QTextEdit, QTabWidget
from PyQt6.QtCore import Qt, QRect, QRectF, QSize, QTimer, pyqtSignal
from PyQt6.QtGui import (
    QPainter, QColor, QTextFormat, QFont, QTextCursor, QPainterPath,
)

from forge_ide.app.syntax import PlainHighlighter
from forge_ide.app.themes import UITheme, SyntaxColors
from forge_ide.app.settings import SettingsManager
from forge_ide.app.block_structure import compute_scopes, line_opens_block


class LineNumberArea(QWidget):
    """Widget for displaying line numbers in the editor gutter"""

    def __init__(self, editor):
        super().__init__(editor)
        self.editor = editor

    def sizeHint(self) -> QSize:
        return QSize(self.editor.line_number_area_width(), 0)

    def paintEvent(self, event):
        self.editor.line_number_area_paint_event(event)


class CodeEditor(QPlainTextEdit):
    """Code editor with line numbers and syntax highlighting"""

    file_modified = pyqtSignal(bool)

    def __init__(self, parent=None, ui_theme: UITheme = None,
                 syntax_theme: SyntaxColors = None, settings: SettingsManager = None):
        super().__init__(parent)
        self.ui_theme = ui_theme
        self.syntax_theme = syntax_theme
        self.settings = settings
        self.file_path = None
        self._modified = False

        # Nested scope boxes: (start_line, end_line, depth) tuples, recomputed
        # on a debounce timer as the document changes (see _on_text_changed).
        self._scopes: list = []
        self._scope_timer = QTimer(self)
        self._scope_timer.setSingleShot(True)
        self._scope_timer.setInterval(200)
        self._scope_timer.timeout.connect(self._recompute_scopes)

        # Create line number area
        self.line_number_area = LineNumberArea(self)

        # Create syntax highlighter
        self.highlighter = PlainHighlighter(self.document(), syntax_theme)

        # Connect signals
        self.blockCountChanged.connect(self.update_line_number_area_width)
        self.updateRequest.connect(self.update_line_number_area)
        self.cursorPositionChanged.connect(self.highlight_current_line)
        self.textChanged.connect(self._on_text_changed)

        # Initial setup
        self.update_line_number_area_width(0)
        self.highlight_current_line()
        self.apply_settings()
        self._recompute_scopes()
    
    def apply_settings(self):
        """Apply editor settings"""
        if self.settings:
            s = self.settings.settings.editor

            # Set font programmatically - this is critical for immediate update
            font = QFont(s.font_family, s.font_size)
            font.setStyleHint(QFont.StyleHint.Monospace)
            self.setFont(font)

            # Also set font on line number area
            self.line_number_area.setFont(font)

            # Update tab stop distance
            self.setTabStopDistance(s.tab_width * self.fontMetrics().horizontalAdvance(' '))

            # Update word wrap
            self.setLineWrapMode(
                QPlainTextEdit.LineWrapMode.WidgetWidth if s.word_wrap
                else QPlainTextEdit.LineWrapMode.NoWrap
            )

            # Update line number visibility
            self.line_number_area.setVisible(s.show_line_numbers)
            self.update_line_number_area_width(0)

            # Re-highlight current line based on settings
            self.highlight_current_line()

            # Re-apply UI theme to update stylesheet with new font settings
            if self.ui_theme:
                self.apply_ui_theme(self.ui_theme)

            # Force immediate visual update
            self.update()
            self.viewport().update()
            self.line_number_area.update()
    
    def apply_ui_theme(self, ui_theme: UITheme):
        """Apply UI theme to editor"""
        self.ui_theme = ui_theme

        # Get font settings to include in stylesheet
        font_family = "monospace"
        font_size = 12
        if self.settings and self.settings.settings.editor:
            font_family = self.settings.settings.editor.font_family
            font_size = self.settings.settings.editor.font_size

        # Set editor colors and font via stylesheet
        self.setStyleSheet(f"""
            QPlainTextEdit {{
                background-color: {ui_theme.editor_background};
                color: {ui_theme.editor_foreground};
                selection-background-color: {ui_theme.editor_selection};
                border: none;
                font-family: "{font_family}";
                font-size: {font_size}pt;
            }}
        """)

        # Update tab stop distance based on current font metrics
        if self.settings and self.settings.settings.editor:
            self.setTabStopDistance(
                self.settings.settings.editor.tab_width *
                self.fontMetrics().horizontalAdvance(' ')
            )

        self.highlight_current_line()
        self.viewport().update()
        self.line_number_area.update()
    
    def apply_syntax_theme(self, syntax_theme: SyntaxColors):
        """Apply syntax theme to editor"""
        self.syntax_theme = syntax_theme
        self.highlighter.set_syntax_theme(syntax_theme)
    
    def line_number_area_width(self) -> int:
        """Calculate width needed for the line number gutter"""
        digits = len(str(max(1, self.blockCount())))
        space = 8 + self.fontMetrics().horizontalAdvance('9') * digits
        return space
    
    def update_line_number_area_width(self, _):
        """Update editor margins for line number area"""
        self.setViewportMargins(self.line_number_area_width(), 0, 0, 0)
    
    def update_line_number_area(self, rect, dy):
        """Update line number area when editor scrolls"""
        if dy:
            self.line_number_area.scroll(0, dy)
        else:
            self.line_number_area.update(0, rect.y(), self.line_number_area.width(), rect.height())
        
        if rect.contains(self.viewport().rect()):
            self.update_line_number_area_width(0)
    
    def keyPressEvent(self, event):
        """Handle key press events for auto-indentation"""
        if event.key() == Qt.Key.Key_Return or event.key() == Qt.Key.Key_Enter:
            # Check previous line for indentation logic
            cursor = self.textCursor()
            block = cursor.block()
            text = block.text()
            
            # Allow default behavior to create the new line
            super().keyPressEvent(event)
            
            # Now determine indentation for the new line
            cursor = self.textCursor()
            prev_block = cursor.block().previous()
            
            if prev_block.isValid():
                prev_text = prev_block.text().rstrip()
                
                # Calculate indentation of previous line
                indent = len(prev_text) - len(prev_text.lstrip())

                # Shared with the scope-box renderer, so auto-indent and the
                # visual nested boxes always agree on where a block starts.
                if line_opens_block(prev_text):
                    indent += 4
                
                # Apply indentation
                if indent > 0:
                    cursor.insertText(" " * indent)
        else:
            super().keyPressEvent(event)

    def resizeEvent(self, event):
        """Handle resize events"""
        super().resizeEvent(event)
        cr = self.contentsRect()
        self.line_number_area.setGeometry(QRect(cr.left(), cr.top(),
                                                 self.line_number_area_width(), cr.height()))

    def _recompute_scopes(self):
        """Recompute nested scope ranges from the current document text.

        Runs on a debounce timer (see _on_text_changed), not on every
        keystroke. Never raises: this must keep working on mid-edit,
        syntactically broken code.
        """
        try:
            self._scopes = compute_scopes(self.toPlainText())
        except Exception:
            self._scopes = []
        self.viewport().update()

    def _scope_boxes_enabled(self) -> bool:
        """Whether nested scope boxes should currently be drawn"""
        if self.settings and self.settings.settings.editor:
            return getattr(self.settings.settings.editor, "show_scope_boxes", True)
        return True

    def paintEvent(self, event):
        """Paint nested scope boxes behind the text, then the text on top"""
        if self.ui_theme and self._scopes and self._scope_boxes_enabled():
            self._paint_scope_boxes()
        super().paintEvent(event)

    def _paint_scope_boxes(self):
        """Draw nested, colored, rounded boxes behind the text based on
        block nesting depth (BlueJ-style scope highlighting).

        Boxes are drawn BEFORE the text (paintEvent calls this, then calls
        super().paintEvent() afterward) so that syntax-highlighted text,
        the current-line highlight, and bracket-match highlighting all
        render crisply on top of the tinted backgrounds, unaffected.
        """
        colors = None
        if self.settings and self.settings.settings.editor:
            colors = self.settings.settings.editor.scope_box_colors
        if not colors:
            colors = getattr(self.ui_theme, "scope_depth_colors", None)
        if not colors:
            return

        painter = QPainter(self.viewport())
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        left_margin = 4
        inset_per_depth = 7
        fill_alpha = 120
        border_alpha = 210
        viewport_height = self.viewport().height()
        viewport_width = self.viewport().width()

        # Shallow (outer) scopes first so deeper ones paint on top and
        # visually read as nested/inset rather than overlapping flatly.
        for start_line, end_line, depth in sorted(self._scopes, key=lambda s: s[2]):
            start_block = self.document().findBlockByNumber(start_line)
            end_block = self.document().findBlockByNumber(end_line)
            if not start_block.isValid() or not end_block.isValid():
                continue

            top = self.blockBoundingGeometry(start_block).translated(self.contentOffset()).top()
            bottom = self.blockBoundingGeometry(end_block).translated(self.contentOffset()).bottom()

            # Skip boxes that are entirely scrolled out of view.
            if bottom < 0 or top > viewport_height:
                continue

            x = left_margin + depth * inset_per_depth
            width = viewport_width - 2 * x
            if width <= 0:
                continue

            rect = QRectF(x, top + 1, width, bottom - top - 2)
            path = QPainterPath()
            path.addRoundedRect(rect, 6, 6)

            color = QColor(colors[depth % len(colors)])
            color.setAlpha(fill_alpha)
            painter.fillPath(path, color)

            border = QColor(color)
            border.setAlpha(border_alpha)
            painter.setPen(border)
            painter.drawPath(path)

        painter.end()

    def line_number_area_paint_event(self, event):
        """Paint line numbers in the gutter"""
        painter = QPainter(self.line_number_area)

        if self.ui_theme:
            painter.fillRect(event.rect(), QColor(self.ui_theme.editor_gutter_bg))
            text_color = QColor(self.ui_theme.editor_gutter_fg)
        else:
            painter.fillRect(event.rect(), QColor("#1e1e2e"))
            text_color = QColor("#6c7086")

        painter.setPen(text_color)

        block = self.firstVisibleBlock()
        block_number = block.blockNumber()
        top = round(self.blockBoundingGeometry(block).translated(self.contentOffset()).top())
        bottom = top + round(self.blockBoundingRect(block).height())

        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                number = str(block_number + 1)
                painter.drawText(0, top, self.line_number_area.width() - 4,
                               self.fontMetrics().height(),
                               Qt.AlignmentFlag.AlignRight, number)

            block = block.next()
            top = bottom
            bottom = top + round(self.blockBoundingRect(block).height())
            block_number += 1

    def _get_bracket_matches(self):
        """Find and highlight matching brackets"""
        extra_selections = []

        if not self.ui_theme:
            return extra_selections

        cursor = self.textCursor()
        text = self.toPlainText()
        pos = cursor.position()

        # Check if cursor is next to a bracket
        bracket_pairs = {
            '(': ')', ')': '(',
            '[': ']', ']': '[',
            '{': '}', '}': '{'
        }

        # Check character before cursor
        char_before = text[pos - 1] if pos > 0 else ''
        # Check character at cursor
        char_at = text[pos] if pos < len(text) else ''

        bracket_char = None
        bracket_pos = None
        is_opening = False

        if char_before in bracket_pairs:
            bracket_char = char_before
            bracket_pos = pos - 1
            is_opening = char_before in '([{'
        elif char_at in bracket_pairs:
            bracket_char = char_at
            bracket_pos = pos
            is_opening = char_at in '([{'

        if bracket_char and bracket_pos is not None:
            # Find matching bracket
            match_pos = self._find_matching_bracket(text, bracket_pos, bracket_char, is_opening)

            if match_pos is not None:
                # Highlight both brackets
                bracket_color = QColor(self.ui_theme.editor_foreground)
                bracket_color.setAlpha(200)

                # Highlight the bracket at cursor
                selection1 = QTextEdit.ExtraSelection()
                selection1.format.setBackground(QColor("#555555"))
                selection1.format.setForeground(QColor("#FFD700"))  # Gold color
                cursor1 = QTextCursor(self.document())
                cursor1.setPosition(bracket_pos)
                cursor1.movePosition(QTextCursor.MoveOperation.Right, QTextCursor.MoveMode.KeepAnchor)
                selection1.cursor = cursor1
                extra_selections.append(selection1)

                # Highlight the matching bracket
                selection2 = QTextEdit.ExtraSelection()
                selection2.format.setBackground(QColor("#555555"))
                selection2.format.setForeground(QColor("#FFD700"))  # Gold color
                cursor2 = QTextCursor(self.document())
                cursor2.setPosition(match_pos)
                cursor2.movePosition(QTextCursor.MoveOperation.Right, QTextCursor.MoveMode.KeepAnchor)
                selection2.cursor = cursor2
                extra_selections.append(selection2)

        return extra_selections

    def _find_matching_bracket(self, text, pos, bracket, is_opening):
        """Find the position of the matching bracket"""
        bracket_pairs = {
            '(': ')', ')': '(',
            '[': ']', ']': '[',
            '{': '}', '}': '{'
        }

        target = bracket_pairs[bracket]
        direction = 1 if is_opening else -1
        depth = 0

        i = pos
        while 0 <= i < len(text):
            char = text[i]

            if char == bracket:
                depth += 1
            elif char == target:
                depth -= 1
                if depth == 0:
                    return i

            i += direction

        return None

    def highlight_current_line(self):
        """Highlight the current line and matching brackets"""
        extra_selections = []

        # Check if highlighting is enabled in settings
        should_highlight = True
        if self.settings and self.settings.settings.editor:
            should_highlight = self.settings.settings.editor.highlight_current_line

        if not self.isReadOnly() and self.ui_theme and should_highlight:
            selection = QTextEdit.ExtraSelection()
            line_color = QColor(self.ui_theme.editor_line_highlight)
            # Translucent, not opaque: an opaque fill here would completely
            # paint over the nested scope box on whichever line the cursor
            # sits on (extra selections are drawn by super().paintEvent(),
            # which runs after _paint_scope_boxes()), making scope coloring
            # look broken on the exact line the user is looking at.
            line_color.setAlpha(140)
            selection.format.setBackground(line_color)
            selection.format.setProperty(QTextFormat.Property.FullWidthSelection, True)
            selection.cursor = self.textCursor()
            selection.cursor.clearSelection()
            extra_selections.append(selection)

        # Add bracket matching highlights
        if self.settings and self.settings.settings.editor.bracket_matching:
            bracket_selections = self._get_bracket_matches()
            extra_selections.extend(bracket_selections)

        self.setExtraSelections(extra_selections)
    
    def _on_text_changed(self):
        """Handle text changes"""
        if not self._modified:
            self._modified = True
            self.file_modified.emit(True)

        # Recompute scope boxes a short moment after the user stops typing,
        # rather than on every keystroke.
        self._scope_timer.start()

    def set_modified(self, modified: bool):
        """Set the modified state"""
        self._modified = modified
        self.file_modified.emit(modified)
    
    def is_modified(self) -> bool:
        """Check if document has been modified"""
        return self._modified

    def indent_selection(self):
        """Indent the selected lines by one level (4 spaces)"""
        cursor = self.textCursor()
        
        # If no selection, indent current line
        if not cursor.hasSelection():
            cursor.movePosition(QTextCursor.MoveOperation.StartOfLine)
            cursor.insertText("    ")
            return
        
        # Get selection range
        start = cursor.selectionStart()
        end = cursor.selectionEnd()
        
        # Move to start of selection
        cursor.setPosition(start)
        cursor.movePosition(QTextCursor.MoveOperation.StartOfLine)
        start_block = cursor.blockNumber()
        
        # Move to end of selection
        cursor.setPosition(end)
        # If cursor is at start of line, don't include that line
        if cursor.positionInBlock() == 0 and start_block != cursor.blockNumber():
            cursor.movePosition(QTextCursor.MoveOperation.PreviousBlock)
        end_block = cursor.blockNumber()
        
        # Indent each line in the selection
        cursor.beginEditBlock()
        for block_num in range(start_block, end_block + 1):
            cursor.setPosition(self.document().findBlockByNumber(block_num).position())
            cursor.insertText("    ")
        cursor.endEditBlock()
    
    def dedent_selection(self):
        """Dedent the selected lines by one level (up to 4 spaces)"""
        cursor = self.textCursor()
        
        # If no selection, dedent current line
        if not cursor.hasSelection():
            cursor.movePosition(QTextCursor.MoveOperation.StartOfLine)
            block = cursor.block()
            text = block.text()
            # Remove up to 4 leading spaces
            spaces_to_remove = 0
            for char in text[:4]:
                if char == ' ':
                    spaces_to_remove += 1
                else:
                    break
            if spaces_to_remove > 0:
                cursor.movePosition(QTextCursor.MoveOperation.Right, 
                                  QTextCursor.MoveMode.KeepAnchor, spaces_to_remove)
                cursor.removeSelectedText()
            return
        
        # Get selection range
        start = cursor.selectionStart()
        end = cursor.selectionEnd()
        
        # Move to start of selection
        cursor.setPosition(start)
        cursor.movePosition(QTextCursor.MoveOperation.StartOfLine)
        start_block = cursor.blockNumber()
        
        # Move to end of selection
        cursor.setPosition(end)
        # If cursor is at start of line, don't include that line
        if cursor.positionInBlock() == 0 and start_block != cursor.blockNumber():
            cursor.movePosition(QTextCursor.MoveOperation.PreviousBlock)
        end_block = cursor.blockNumber()
        
        # Dedent each line in the selection
        cursor.beginEditBlock()
        for block_num in range(start_block, end_block + 1):
            block = self.document().findBlockByNumber(block_num)
            cursor.setPosition(block.position())
            text = block.text()
            # Remove up to 4 leading spaces
            spaces_to_remove = 0
            for char in text[:4]:
                if char == ' ':
                    spaces_to_remove += 1
                else:
                    break
            if spaces_to_remove > 0:
                cursor.movePosition(QTextCursor.MoveOperation.Right, 
                                  QTextCursor.MoveMode.KeepAnchor, spaces_to_remove)
                cursor.removeSelectedText()
        cursor.endEditBlock()
    
    def comment_selection(self):
        """Convert selected lines to comments (rem: for single line, note: for multi-line)"""
        cursor = self.textCursor()
        
        # If no selection, just insert note: on current line
        if not cursor.hasSelection():
            cursor.movePosition(QTextCursor.MoveOperation.StartOfLine)
            cursor.insertText("note:\n    ")
            return
        
        # Get selection range
        start = cursor.selectionStart()
        end = cursor.selectionEnd()
        
        # Move to start of selection
        cursor.setPosition(start)
        cursor.movePosition(QTextCursor.MoveOperation.StartOfLine)
        start_block = cursor.blockNumber()
        start_pos = cursor.position()
        
        # Move to end of selection
        cursor.setPosition(end)
        # If cursor is at start of line, don't include that line
        if cursor.positionInBlock() == 0 and start_block != cursor.blockNumber():
            cursor.movePosition(QTextCursor.MoveOperation.PreviousBlock)
        cursor.movePosition(QTextCursor.MoveOperation.EndOfLine)
        end_block = cursor.blockNumber()
        
        # Check if single line or multi-line
        is_single_line = (start_block == end_block)
        
        cursor.beginEditBlock()
        
        if is_single_line:
            # Single line - use rem:
            block = self.document().findBlockByNumber(start_block)
            line = block.text()
            indent = len(line) - len(line.lstrip())
            indent_str = " " * indent
            
            # Create rem: comment
            commented_line = indent_str + "rem: " + line.lstrip()
            
            # Replace the line
            cursor.setPosition(start_pos)
            cursor.setPosition(block.position() + len(block.text()),
                              QTextCursor.MoveMode.KeepAnchor)
            cursor.removeSelectedText()
            cursor.insertText(commented_line)
        else:
            # Multi-line - use note: block
            # Get the minimum indentation of selected lines
            min_indent = float('inf')
            for block_num in range(start_block, end_block + 1):
                block = self.document().findBlockByNumber(block_num)
                text = block.text()
                if text.strip():  # Only consider non-empty lines
                    indent = len(text) - len(text.lstrip())
                    min_indent = min(min_indent, indent)
            
            if min_indent == float('inf'):
                min_indent = 0
            
            # Collect all lines
            lines = []
            for block_num in range(start_block, end_block + 1):
                block = self.document().findBlockByNumber(block_num)
                lines.append(block.text())
            
            # Create the note: block
            indent_str = " " * min_indent
            commented_lines = [indent_str + "note:"]
            for line in lines:
                if line.strip():  # Non-empty line
                    # Remove the minimum indentation and add 4 spaces
                    dedented = line[min_indent:] if len(line) >= min_indent else line
                    commented_lines.append(indent_str + "    " + dedented)
                else:
                    # Empty line
                    commented_lines.append("")
            
            # Replace the selection with the commented version
            cursor.setPosition(start_pos)
            cursor.setPosition(self.document().findBlockByNumber(end_block).position() + 
                              len(self.document().findBlockByNumber(end_block).text()),
                              QTextCursor.MoveMode.KeepAnchor)
            cursor.removeSelectedText()
            cursor.insertText("\n".join(commented_lines))
        
        cursor.endEditBlock()


