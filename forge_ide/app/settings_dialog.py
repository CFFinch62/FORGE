"""
Settings Dialog for FORGE IDE
Provides a preferences UI for configuring IDE settings
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QTabWidget, QWidget,
    QLabel, QSpinBox, QCheckBox, QComboBox, QLineEdit,
    QPushButton, QGroupBox, QFormLayout, QTreeWidget, QTreeWidgetItem,
    QHeaderView, QFontComboBox, QFileDialog, QColorDialog
)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFontDatabase, QColor

from forge_ide.app.settings import SettingsManager
from forge_ide.app.themes import ThemeManager


# Keyboard shortcuts reference table
SHORTCUTS = [
    ("File", "New File", "Ctrl+N"),
    ("File", "Open File", "Ctrl+O"),
    ("File", "Open Folder", "Ctrl+Shift+O"),
    ("File", "Save", "Ctrl+S"),
    ("File", "Save As", "Ctrl+Shift+S"),
    ("File", "Exit", "Ctrl+Q"),
    ("Edit", "Undo", "Ctrl+Z"),
    ("Edit", "Redo", "Ctrl+Shift+Z"),
    ("Edit", "Cut", "Ctrl+X"),
    ("Edit", "Copy", "Ctrl+C"),
    ("Edit", "Paste", "Ctrl+V"),
    ("Edit", "Find", "Ctrl+F"),
    ("Edit", "Replace", "Ctrl+H"),
    ("Edit", "Preferences", "Ctrl+,"),
    ("View", "Toggle File Browser", "Ctrl+B"),
    ("View", "Toggle Terminal", "Ctrl+`"),
    ("Run", "Run", "F5"),
    ("Run", "Stop", "Shift+F5"),
    ("Help", "Quick Reference", "F1"),
]


class SettingsDialog(QDialog):
    """Preferences dialog for IDE settings"""

    settings_applied = pyqtSignal()  # Signal emitted when settings are applied

    def __init__(self, settings: SettingsManager, theme_manager: ThemeManager,
                 parent=None):
        super().__init__(parent)
        self.settings_manager = settings
        self.theme_manager = theme_manager
        self._scope_color_override = []
        self.setWindowTitle("Preferences")
        self.setMinimumSize(500, 450)
        self._setup_ui()
        self._load_current_settings()

    def _setup_ui(self):
        """Set up the dialog UI"""
        layout = QVBoxLayout(self)

        # Tab widget
        self.tabs = QTabWidget()

        # Editor tab
        self.tabs.addTab(self._create_editor_tab(), "Editor")

        # Theme tab
        self.tabs.addTab(self._create_theme_tab(), "Theme")

        # Terminal tab
        self.tabs.addTab(self._create_terminal_tab(), "Terminal")

        # Shortcuts tab
        self.tabs.addTab(self._create_shortcuts_tab(), "Shortcuts")

        # Runtime tab
        self.tabs.addTab(self._create_runtime_tab(), "Runtime")

        layout.addWidget(self.tabs)

        # Buttons
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()

        cancel_btn = QPushButton("Cancel")
        cancel_btn.clicked.connect(self.reject)
        btn_layout.addWidget(cancel_btn)

        apply_btn = QPushButton("Apply")
        apply_btn.clicked.connect(self._apply_settings)
        btn_layout.addWidget(apply_btn)

        ok_btn = QPushButton("OK")
        ok_btn.clicked.connect(self._ok_clicked)
        btn_layout.addWidget(ok_btn)

        layout.addLayout(btn_layout)

    def _create_editor_tab(self) -> QWidget:
        """Create the editor settings tab"""
        tab = QWidget()
        layout = QVBoxLayout(tab)

        # Font group
        font_group = QGroupBox("Font")
        font_layout = QFormLayout()

        self.font_family_input = QFontComboBox()
        self.font_family_input.setFontFilters(QFontComboBox.FontFilter.MonospacedFonts)
        font_layout.addRow("Font Family:", self.font_family_input)

        self.font_size_spin = QSpinBox()
        self.font_size_spin.setRange(8, 32)
        font_layout.addRow("Font Size:", self.font_size_spin)

        font_group.setLayout(font_layout)
        layout.addWidget(font_group)

        # Editor behavior group
        behavior_group = QGroupBox("Behavior")
        behavior_layout = QVBoxLayout()

        self.tab_width_spin = QSpinBox()
        self.tab_width_spin.setRange(2, 8)
        tab_row = QHBoxLayout()
        tab_row.addWidget(QLabel("Tab Width:"))
        tab_row.addWidget(self.tab_width_spin)
        tab_row.addStretch()
        behavior_layout.addLayout(tab_row)

        self.word_wrap_check = QCheckBox("Word Wrap")
        behavior_layout.addWidget(self.word_wrap_check)

        self.line_numbers_check = QCheckBox("Show Line Numbers")
        behavior_layout.addWidget(self.line_numbers_check)

        self.highlight_line_check = QCheckBox("Highlight Current Line")
        behavior_layout.addWidget(self.highlight_line_check)

        self.bracket_matching_check = QCheckBox("Bracket Matching")
        behavior_layout.addWidget(self.bracket_matching_check)

        behavior_group.setLayout(behavior_layout)
        layout.addWidget(behavior_group)

        # Nested scope coloring group
        scope_group = QGroupBox("Nested Scope Coloring")
        scope_layout = QVBoxLayout()

        self.scope_boxes_check = QCheckBox("Show nested scope boxes")
        scope_layout.addWidget(self.scope_boxes_check)

        colors_row = QHBoxLayout()
        colors_row.addWidget(QLabel("Depth colors:"))
        self._scope_color_buttons = []
        for depth in range(4):
            btn = QPushButton()
            btn.setFixedSize(32, 22)
            btn.setToolTip(f"Depth {depth + 1} color")
            btn.clicked.connect(lambda checked, d=depth: self._pick_scope_color(d))
            colors_row.addWidget(btn)
            self._scope_color_buttons.append(btn)
        colors_row.addStretch()
        scope_layout.addLayout(colors_row)

        reset_scope_btn = QPushButton("Reset to Theme Defaults")
        reset_scope_btn.clicked.connect(self._reset_scope_colors)
        scope_layout.addWidget(reset_scope_btn)

        scope_group.setLayout(scope_layout)
        layout.addWidget(scope_group)

        layout.addStretch()
        return tab

    def _create_theme_tab(self) -> QWidget:
        """Create the theme settings tab"""
        tab = QWidget()
        layout = QVBoxLayout(tab)

        # UI Theme group
        ui_theme_group = QGroupBox("UI Theme")
        ui_theme_layout = QVBoxLayout()

        self.ui_theme_combo = QComboBox()
        for name in self.theme_manager.get_available_ui_themes():
            self.ui_theme_combo.addItem(name.replace('_', ' ').title(), name)
        ui_theme_layout.addWidget(self.ui_theme_combo)

        ui_theme_group.setLayout(ui_theme_layout)
        layout.addWidget(ui_theme_group)

        # Syntax Theme group
        syntax_theme_group = QGroupBox("Syntax Theme")
        syntax_theme_layout = QVBoxLayout()

        self.syntax_theme_combo = QComboBox()
        for name in self.theme_manager.get_available_syntax_themes():
            self.syntax_theme_combo.addItem(name.replace('_', ' ').title(), name)
        syntax_theme_layout.addWidget(self.syntax_theme_combo)

        syntax_theme_group.setLayout(syntax_theme_layout)
        layout.addWidget(syntax_theme_group)

        # Theme preview
        preview_group = QGroupBox("Preview")
        preview_layout = QVBoxLayout()
        
        self.preview_label = QLabel()
        self.preview_label.setFixedHeight(100)
        self.preview_label.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
        self.preview_label.setWordWrap(True)
        self._update_preview()
        self.ui_theme_combo.currentIndexChanged.connect(self._update_preview)
        self.syntax_theme_combo.currentIndexChanged.connect(self._update_preview)
        # Keep the scope-color swatches (Editor tab) in sync with whichever
        # UI theme is selected here, for anyone who hasn't customized them.
        self.ui_theme_combo.currentIndexChanged.connect(self._update_scope_color_buttons)
        preview_layout.addWidget(self.preview_label)
        
        preview_group.setLayout(preview_layout)
        layout.addWidget(preview_group)

        layout.addStretch()
        return tab

    def _create_terminal_tab(self) -> QWidget:
        """Create the terminal settings tab"""
        tab = QWidget()
        layout = QVBoxLayout(tab)

        font_group = QGroupBox("Terminal Font")
        font_layout = QFormLayout()

        self.terminal_font_input = QFontComboBox()
        self.terminal_font_input.setFontFilters(QFontComboBox.FontFilter.MonospacedFonts)
        font_layout.addRow("Font Family:", self.terminal_font_input)

        self.terminal_font_size_spin = QSpinBox()
        self.terminal_font_size_spin.setRange(8, 24)
        font_layout.addRow("Font Size:", self.terminal_font_size_spin)

        font_group.setLayout(font_layout)
        layout.addWidget(font_group)

        # External Terminal group
        ext_term_group = QGroupBox("External Terminal")
        ext_term_layout = QVBoxLayout()

        lbl = QLabel("Command to launch external terminal:")
        ext_term_layout.addWidget(lbl)

        cmd_layout = QHBoxLayout()
        self.ext_term_input = QLineEdit()
        self.ext_term_input.setPlaceholderText("e.g. gnome-terminal --, xterm -e, etc.")
        cmd_layout.addWidget(self.ext_term_input)

        detect_btn = QPushButton("Auto-detect")
        detect_btn.clicked.connect(self._auto_detect_terminal)
        cmd_layout.addWidget(detect_btn)

        ext_term_layout.addLayout(cmd_layout)

        help_lbl = QLabel("Use {command} as placeholder for the command to run.\nExample: gnome-terminal -- {command}")
        help_lbl.setStyleSheet("color: gray; font-style: italic;")
        ext_term_layout.addWidget(help_lbl)

        ext_term_group.setLayout(ext_term_layout)
        layout.addWidget(ext_term_group)

        layout.addStretch()
        return tab

    def _create_shortcuts_tab(self) -> QWidget:
        """Create the keyboard shortcuts tab (read-only reference)"""
        tab = QWidget()
        layout = QVBoxLayout(tab)

        info_label = QLabel("Current keyboard shortcuts:")
        layout.addWidget(info_label)

        tree = QTreeWidget()
        tree.setHeaderLabels(["Action", "Shortcut", "Category"])
        tree.setRootIsDecorated(False)
        tree.setAlternatingRowColors(True)
        tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        tree.header().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)

        for category, action, shortcut in SHORTCUTS:
            item = QTreeWidgetItem([action, shortcut, category])
            tree.addTopLevelItem(item)

        layout.addWidget(tree)
        return tab

    def _create_runtime_tab(self) -> QWidget:
        """Create the runtime/interpreter settings tab"""
        tab = QWidget()
        layout = QVBoxLayout(tab)

        # Interpreter group
        interpreter_group = QGroupBox("FORGE Interpreter")
        interpreter_layout = QVBoxLayout()

        lbl = QLabel("Path to 'forge' executable:")
        interpreter_layout.addWidget(lbl)

        path_layout = QHBoxLayout()
        self.interpreter_path_input = QLineEdit()
        self.interpreter_path_input.setPlaceholderText("Auto-detect")
        path_layout.addWidget(self.interpreter_path_input)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_interpreter)
        path_layout.addWidget(browse_btn)

        interpreter_layout.addLayout(path_layout)

        info_lbl = QLabel("Leave empty to auto-detect the interpreter in your PATH or program directory.")
        info_lbl.setWordWrap(True)
        info_lbl.setStyleSheet("color: gray; font-style: italic;")
        interpreter_layout.addWidget(info_lbl)

        interpreter_group.setLayout(interpreter_layout)
        layout.addWidget(interpreter_group)

        layout.addStretch()
        return tab

    def _browse_interpreter(self):
        """Open file dialog to select interpreter"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select FORGE Interpreter", "",
            "Executable (*);;All Files (*)"
        )
        if file_path:
            self.interpreter_path_input.setText(file_path)

    def _load_current_settings(self):
        """Load current settings into the dialog"""
        s = self.settings_manager.settings

        # Editor
        self.font_family_input.setCurrentText(s.editor.font_family)
        self.font_size_spin.setValue(s.editor.font_size)
        self.tab_width_spin.setValue(s.editor.tab_width)
        self.word_wrap_check.setChecked(s.editor.word_wrap)
        self.line_numbers_check.setChecked(s.editor.show_line_numbers)
        self.highlight_line_check.setChecked(s.editor.highlight_current_line)
        self.bracket_matching_check.setChecked(s.editor.bracket_matching)

        self.scope_boxes_check.setChecked(s.editor.show_scope_boxes)
        # Empty override means "inherit from the current UI theme" -- keep
        # that distinction in memory so switching themes later keeps
        # driving the colors for anyone who never customizes them.
        self._scope_color_override = list(s.editor.scope_box_colors)
        self._update_scope_color_buttons()

        # Theme - load both UI and syntax themes
        ui_theme = getattr(s.theme, 'ui_theme', s.theme.current_theme)
        syntax_theme = getattr(s.theme, 'syntax_theme', 'default')
        
        ui_idx = self.ui_theme_combo.findData(ui_theme)
        if ui_idx >= 0:
            self.ui_theme_combo.setCurrentIndex(ui_idx)
        
        syntax_idx = self.syntax_theme_combo.findData(syntax_theme)
        if syntax_idx >= 0:
            self.syntax_theme_combo.setCurrentIndex(syntax_idx)

        # Terminal
        self.terminal_font_input.setCurrentText(s.terminal.font_family)
        self.terminal_font_size_spin.setValue(s.terminal.font_size)
        self.ext_term_input.setText(s.terminal.external_terminal_command)

        # Runtime
        self.interpreter_path_input.setText(s.forge_interpreter_path)

    def _update_preview(self):
        """Update the theme preview swatch"""
        ui_theme_name = self.ui_theme_combo.currentData()
        syntax_theme_name = self.syntax_theme_combo.currentData()
        
        if ui_theme_name and syntax_theme_name:
            ui_theme = self.theme_manager.get_ui_theme(ui_theme_name)
            syntax_theme = self.theme_manager.get_syntax_theme(syntax_theme_name)
            
            self.preview_label.setStyleSheet(f"""
                background-color: {ui_theme.editor_background};
                color: {ui_theme.editor_foreground};
                border: 1px solid {ui_theme.panel_border};
                border-radius: 6px;
                padding: 12px;
                font-family: monospace;
                font-size: 11px;
            """)
            
            # Show sample code with syntax highlighting colors
            sample_html = f'''
                <span style="color: {syntax_theme.keyword};">proc</span>
                <span style="color: {syntax_theme.function};">greet</span>
                <span style="color: {ui_theme.foreground};">(</span>
                <span style="color: {syntax_theme.identifier};">name</span>
                <span style="color: {syntax_theme.operator};">:</span>
                <span style="color: {syntax_theme.type};">str</span>
                <span style="color: {ui_theme.foreground};">)</span><br>
                &nbsp;&nbsp;<span style="color: {syntax_theme.builtin};">println</span>
                <span style="color: {ui_theme.foreground};">(</span>
                <span style="color: {syntax_theme.string};">"Hello, FORGE!"</span>
                <span style="color: {ui_theme.foreground};">)</span><br>
                <span style="color: {syntax_theme.comment};"># Theme preview</span>
            '''
            self.preview_label.setText(sample_html)

    def _apply_settings(self):
        """Apply settings without closing"""
        s = self.settings_manager.settings

        # Editor
        s.editor.font_family = self.font_family_input.currentText()
        s.editor.font_size = self.font_size_spin.value()
        s.editor.tab_width = self.tab_width_spin.value()
        s.editor.word_wrap = self.word_wrap_check.isChecked()
        s.editor.show_line_numbers = self.line_numbers_check.isChecked()
        s.editor.highlight_current_line = self.highlight_line_check.isChecked()
        s.editor.bracket_matching = self.bracket_matching_check.isChecked()

        s.editor.show_scope_boxes = self.scope_boxes_check.isChecked()
        s.editor.scope_box_colors = list(self._scope_color_override)

        # Theme - save both UI and syntax themes
        new_ui_theme = self.ui_theme_combo.currentData()
        new_syntax_theme = self.syntax_theme_combo.currentData()

        if new_ui_theme:
            s.theme.ui_theme = new_ui_theme
            s.theme.current_theme = new_ui_theme  # Keep for backward compatibility
            self.theme_manager.set_ui_theme(new_ui_theme)

        if new_syntax_theme:
            s.theme.syntax_theme = new_syntax_theme
            self.theme_manager.set_syntax_theme(new_syntax_theme)

        # Terminal
        s.terminal.font_family = self.terminal_font_input.currentText()
        s.terminal.font_size = self.terminal_font_size_spin.value()
        s.terminal.external_terminal_command = self.ext_term_input.text().strip()

        # Runtime
        s.forge_interpreter_path = self.interpreter_path_input.text().strip()

        self.settings_manager.save()

        # Emit signal so main window can apply settings immediately
        self.settings_applied.emit()

    def _current_theme_scope_defaults(self) -> list:
        """Scope depth colors of the UI theme currently selected in this dialog"""
        theme_name = self.ui_theme_combo.currentData()
        ui_theme = self.theme_manager.get_ui_theme(theme_name) if theme_name else \
            self.theme_manager.get_current_ui_theme()
        return list(getattr(ui_theme, "scope_depth_colors", []))

    def _update_scope_color_buttons(self):
        """Refresh the color swatch buttons from the override (or theme defaults)"""
        colors = self._scope_color_override or self._current_theme_scope_defaults()
        for i, btn in enumerate(self._scope_color_buttons):
            color = colors[i % len(colors)] if colors else "#888888"
            btn.setStyleSheet(
                f"background-color: {color}; border: 1px solid #00000040; border-radius: 3px;"
            )

    def _pick_scope_color(self, depth: int):
        """Open a color picker for one nesting depth and store the override"""
        colors = self._scope_color_override or self._current_theme_scope_defaults()
        current = QColor(colors[depth % len(colors)]) if colors else QColor("#888888")

        chosen = QColorDialog.getColor(current, self, f"Depth {depth + 1} Scope Color")
        if not chosen.isValid():
            return

        if not self._scope_color_override:
            self._scope_color_override = self._current_theme_scope_defaults()
        # Grow the list if the theme has fewer depths than the button being edited
        while len(self._scope_color_override) <= depth:
            self._scope_color_override.append("#888888")
        self._scope_color_override[depth] = chosen.name()
        self._update_scope_color_buttons()

    def _reset_scope_colors(self):
        """Clear the override so scope colors follow the UI theme again"""
        self._scope_color_override = []
        self._update_scope_color_buttons()

    def _auto_detect_terminal(self):
        """Auto-detect the best external terminal command"""
        import shutil
        import sys

        command = ""

        if sys.platform == "win32":
            command = "start cmd /k"
        elif sys.platform == "darwin":
            command = "open -a Terminal"
        else:
            # Linux/Unix
            terminals = [
                ("gnome-terminal", "gnome-terminal --"),
                ("konsole", "konsole -e"),
                ("xfce4-terminal", "xfce4-terminal -x"),
                ("x-terminal-emulator", "x-terminal-emulator -e"),
                ("xterm", "xterm -e"),
                ("urxvt", "urxvt -e"),
            ]

            for term, cmd_template in terminals:
                if shutil.which(term):
                    command = cmd_template
                    break

        if command:
            self.ext_term_input.setText(command)
        else:
            self.ext_term_input.setPlaceholderText("No supported terminal found")

    def _ok_clicked(self):
        """Apply and close"""
        self._apply_settings()
        self.accept()

    def get_selected_ui_theme(self) -> str:
        """Get the UI theme name selected in the dialog"""
        return self.ui_theme_combo.currentData() or "dark"
    
    def get_selected_syntax_theme(self) -> str:
        """Get the syntax theme name selected in the dialog"""
        return self.syntax_theme_combo.currentData() or "default"
