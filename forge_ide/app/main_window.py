"""
Main Window for FORGE IDE
The primary application window containing all IDE components
"""

from pathlib import Path
from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QSplitter,
    QTabWidget, QMenuBar, QMenu, QToolBar, QStatusBar, QFileDialog,
    QMessageBox, QLabel, QStyle
)
from PyQt6.QtCore import Qt, QSize
from PyQt6.QtGui import QAction, QKeySequence, QIcon

import sys
import os

from forge_ide.app.settings import SettingsManager
from forge_ide.app.themes import ThemeManager
from forge_ide.app.editor import CodeEditor
from forge_ide.app.file_browser import FileBrowserWidget
from forge_ide.app.terminal import TerminalWidget
from forge_ide.app.find_replace import FindReplaceWidget
from forge_ide.app.settings_dialog import SettingsDialog
from forge_ide.app.help_viewer import HelpViewer
from forge_ide.app.utils import get_resource_path


class ForgeIDEMainWindow(QMainWindow):
    """Main window for the FORGE IDE"""

    def __init__(self, settings: SettingsManager, theme_manager: ThemeManager):
        super().__init__()
        self.settings = settings
        self.theme_manager = theme_manager
        self.editors = {}  # path -> CodeEditor
        self.current_project_path = None
        
        self._setup_ui()
        self._setup_menus()
        self._setup_toolbar()
        self._setup_statusbar()
        self._apply_settings()
        self._apply_theme()
        self._restore_session()
    
    def _setup_ui(self):
        """Set up the main UI layout"""
        self.setWindowTitle("FORGE IDE")

        # Set window icon
        icon_path = get_resource_path("images/forge_icon_256.png")
        if icon_path and Path(icon_path).exists():
            self.setWindowIcon(QIcon(icon_path))
        
        # Central widget with main layout
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        
        # Main horizontal splitter (file browser | editor + terminal)
        self.main_splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # File browser (with bookmarks)
        # Note: file_browser, terminal, find_replace use a compatibility Theme object
        # They don't need syntax highlighting
        self.file_browser = FileBrowserWidget(
            theme=self._get_compat_theme(),
            settings=self.settings
        )
        self.file_browser.file_double_clicked.connect(self.open_file)
        self.file_browser.bookmark_navigated.connect(self._on_bookmark_navigated)
        self.main_splitter.addWidget(self.file_browser)
        
        # Middle: editor + terminal/debug in vertical splitter
        self.middle_splitter = QSplitter(Qt.Orientation.Vertical)

        # Editor area: tabs + find/replace bar
        editor_container = QWidget()
        editor_layout = QVBoxLayout(editor_container)
        editor_layout.setContentsMargins(0, 0, 0, 0)
        editor_layout.setSpacing(0)

        # Tab widget for editors
        self.tab_widget = QTabWidget()
        self.tab_widget.setTabsClosable(True)
        self.tab_widget.setMovable(True)
        self.tab_widget.tabCloseRequested.connect(self.close_tab)
        self.tab_widget.currentChanged.connect(self._on_tab_changed)
        editor_layout.addWidget(self.tab_widget)

        # Find/Replace bar (hidden by default)
        self.find_replace = FindReplaceWidget(theme=self._get_compat_theme())
        editor_layout.addWidget(self.find_replace)

        self.middle_splitter.addWidget(editor_container)

        # Terminal
        self.terminal = TerminalWidget(
            theme=self._get_compat_theme(),
            settings=self.settings
        )
        self.middle_splitter.addWidget(self.terminal)

        # Set splitter proportions
        self.middle_splitter.setSizes([600, 200])

        self.main_splitter.addWidget(self.middle_splitter)

        self.main_splitter.setSizes([250, 750])
        
        main_layout.addWidget(self.main_splitter)
    
    def _setup_menus(self):
        """Set up the menu bar"""
        menubar = self.menuBar()
        
        # File menu
        file_menu = menubar.addMenu("&File")
        
        new_action = QAction(self._std_icon(QStyle.StandardPixmap.SP_FileIcon), "New", self)
        new_action.setShortcut(QKeySequence.StandardKey.New)
        new_action.triggered.connect(self.new_file)
        file_menu.addAction(new_action)

        open_action = QAction(self._std_icon(QStyle.StandardPixmap.SP_DirOpenIcon), "Open File...", self)
        open_action.setShortcut(QKeySequence.StandardKey.Open)
        open_action.triggered.connect(self.open_file_dialog)
        file_menu.addAction(open_action)

        open_folder_action = QAction(self._std_icon(QStyle.StandardPixmap.SP_DirIcon), "Open Folder...", self)
        open_folder_action.setShortcut("Ctrl+Shift+O")
        open_folder_action.triggered.connect(self.open_folder_dialog)
        file_menu.addAction(open_folder_action)

        file_menu.addSeparator()

        save_action = QAction(self._std_icon(QStyle.StandardPixmap.SP_DialogSaveButton), "Save", self)
        save_action.setShortcut(QKeySequence.StandardKey.Save)
        save_action.triggered.connect(self.save_file)
        file_menu.addAction(save_action)
        
        save_as_action = QAction("Save As...", self)
        save_as_action.setShortcut("Ctrl+Shift+S")
        save_as_action.triggered.connect(self.save_file_as)
        file_menu.addAction(save_as_action)
        
        file_menu.addSeparator()
        
        exit_action = QAction("Exit", self)
        exit_action.setShortcut("Ctrl+Q")
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
        
        # Edit menu
        edit_menu = menubar.addMenu("&Edit")
        
        undo_action = QAction("Undo", self)
        undo_action.setShortcut(QKeySequence.StandardKey.Undo)
        undo_action.triggered.connect(self._undo)
        edit_menu.addAction(undo_action)
        
        redo_action = QAction("Redo", self)
        redo_action.setShortcut(QKeySequence.StandardKey.Redo)
        redo_action.triggered.connect(self._redo)
        edit_menu.addAction(redo_action)
        
        edit_menu.addSeparator()
        
        cut_action = QAction("Cut", self)
        cut_action.setShortcut(QKeySequence.StandardKey.Cut)
        cut_action.triggered.connect(self._cut)
        edit_menu.addAction(cut_action)
        
        copy_action = QAction("Copy", self)
        copy_action.setShortcut(QKeySequence.StandardKey.Copy)
        copy_action.triggered.connect(self._copy)
        edit_menu.addAction(copy_action)
        
        paste_action = QAction("Paste", self)
        paste_action.setShortcut(QKeySequence.StandardKey.Paste)
        paste_action.triggered.connect(self._paste)
        edit_menu.addAction(paste_action)

        edit_menu.addSeparator()

        find_action = QAction("Find...", self)
        find_action.setShortcut("Ctrl+F")
        find_action.triggered.connect(self._show_find)
        edit_menu.addAction(find_action)

        replace_action = QAction("Replace...", self)
        replace_action.setShortcut("Ctrl+H")
        replace_action.triggered.connect(self._show_replace)
        edit_menu.addAction(replace_action)

        edit_menu.addSeparator()
        
        # Code editing actions
        indent_action = QAction("Indent Selection", self)
        indent_action.setShortcuts(["Tab", "Ctrl+]"])
        indent_action.triggered.connect(self._indent_selection)
        edit_menu.addAction(indent_action)
        
        dedent_action = QAction("Dedent Selection", self)
        dedent_action.setShortcuts(["Shift+Tab", "Ctrl+["])
        dedent_action.triggered.connect(self._dedent_selection)
        edit_menu.addAction(dedent_action)
        
        comment_action = QAction("Comment Selection", self)
        comment_action.setShortcut("Ctrl+/")
        comment_action.triggered.connect(self._comment_selection)
        edit_menu.addAction(comment_action)

        edit_menu.addSeparator()

        prefs_action = QAction("Preferences...", self)
        prefs_action.setShortcut("Ctrl+,")
        prefs_action.triggered.connect(self._show_preferences)
        edit_menu.addAction(prefs_action)

        # View menu
        view_menu = menubar.addMenu("&View")

        toggle_browser = QAction(self._std_icon(QStyle.StandardPixmap.SP_DirIcon), "Toggle File Browser", self)
        toggle_browser.setShortcut("Ctrl+B")
        toggle_browser.triggered.connect(self._toggle_file_browser)
        view_menu.addAction(toggle_browser)

        toggle_terminal = QAction(self._std_icon(QStyle.StandardPixmap.SP_ComputerIcon), "Toggle Terminal", self)
        toggle_terminal.setShortcut("Ctrl+`")
        toggle_terminal.triggered.connect(self._toggle_terminal)
        view_menu.addAction(toggle_terminal)

        view_menu.addSeparator()

        # Theme submenu - now shows UI themes and syntax themes
        theme_menu = view_menu.addMenu("Theme")
        
        # UI Theme submenu
        ui_theme_menu = theme_menu.addMenu("UI Theme")
        for theme_name in self.theme_manager.get_available_ui_themes():
            theme_action = QAction(theme_name.replace('_', ' ').title(), self)
            theme_action.triggered.connect(lambda checked, n=theme_name: self._set_ui_theme(n))
            ui_theme_menu.addAction(theme_action)
        
        # Syntax Theme submenu
        # We need to store this menu to update it later when UI theme changes
        self.syntax_theme_menu = theme_menu.addMenu("Syntax Theme")
        self._update_syntax_theme_menu()

        
        view_menu.addSeparator()
        
        # Terminal Position submenu
        terminal_pos_menu = view_menu.addMenu("Terminal Position")
        
        terminal_bottom_action = QAction("Bottom", self)
        terminal_bottom_action.triggered.connect(lambda: self._set_terminal_position("bottom"))
        terminal_pos_menu.addAction(terminal_bottom_action)
        
        terminal_right_action = QAction("Right", self)
        terminal_right_action.triggered.connect(lambda: self._set_terminal_position("right"))
        terminal_pos_menu.addAction(terminal_right_action)

        # Run menu
        run_menu = menubar.addMenu("&Run")

        run_action = QAction(self._std_icon(QStyle.StandardPixmap.SP_MediaPlay), "Run", self)
        run_action.setShortcut("F5")
        run_action.triggered.connect(self.run_current_file)
        run_menu.addAction(run_action)

        run_ext_action = QAction(self._std_icon(QStyle.StandardPixmap.SP_ComputerIcon), "Run in External Terminal", self)
        run_ext_action.setShortcut("Ctrl+F5")
        run_ext_action.triggered.connect(self.run_current_file_external)
        run_menu.addAction(run_ext_action)

        stop_action = QAction(self._std_icon(QStyle.StandardPixmap.SP_MediaStop), "Stop", self)
        stop_action.setShortcut("Shift+F5")
        stop_action.triggered.connect(self.terminal.stop_execution)
        run_menu.addAction(stop_action)

        run_menu.addSeparator()

        set_interpreter_action = QAction("Set Interpreter...", self)
        set_interpreter_action.triggered.connect(self._show_interpreter_settings)
        run_menu.addAction(set_interpreter_action)

        # Help menu
        help_menu = menubar.addMenu("&Help")

        quick_ref_action = QAction("FORGE Quick Reference", self)
        quick_ref_action.setShortcut("F1")
        quick_ref_action.triggered.connect(self._show_quick_reference)
        help_menu.addAction(quick_ref_action)

        help_menu.addSeparator()

        about_action = QAction("About FORGE IDE", self)
        about_action.triggered.connect(self._show_about)
        help_menu.addAction(about_action)

    def _std_icon(self, standard_pixmap):
        """Get a standard icon from the application style"""
        return self.style().standardIcon(standard_pixmap)

    def _setup_toolbar(self):
        """Set up the toolbar"""
        toolbar = QToolBar("Main Toolbar")
        toolbar.setMovable(False)
        toolbar.setIconSize(QSize(24, 24))
        self.addToolBar(toolbar)

        # New file
        new_btn = QAction(self._std_icon(QStyle.StandardPixmap.SP_FileIcon), "New", self)
        new_btn.setToolTip("New File (Ctrl+N)")
        new_btn.triggered.connect(self.new_file)
        toolbar.addAction(new_btn)

        # Open
        open_btn = QAction(self._std_icon(QStyle.StandardPixmap.SP_DirOpenIcon), "Open", self)
        open_btn.setToolTip("Open File (Ctrl+O)")
        open_btn.triggered.connect(self.open_file_dialog)
        toolbar.addAction(open_btn)

        # Save
        save_btn = QAction(self._std_icon(QStyle.StandardPixmap.SP_DialogSaveButton), "Save", self)
        save_btn.setToolTip("Save (Ctrl+S)")
        save_btn.triggered.connect(self.save_file)
        toolbar.addAction(save_btn)

        toolbar.addSeparator()

        # Run
        run_btn = QAction(self._std_icon(QStyle.StandardPixmap.SP_MediaPlay), "Run", self)
        run_btn.setToolTip("Run (F5)")
        run_btn.triggered.connect(self.run_current_file)
        toolbar.addAction(run_btn)

        # Stop
        stop_btn = QAction(self._std_icon(QStyle.StandardPixmap.SP_MediaStop), "Stop", self)
        stop_btn.setToolTip("Stop (Shift+F5)")
        stop_btn.triggered.connect(self.terminal.stop_execution)
        toolbar.addAction(stop_btn)

    def _setup_statusbar(self):
        """Set up the status bar"""
        self.statusbar = QStatusBar()
        self.setStatusBar(self.statusbar)

        self.file_label = QLabel("No file open")
        self.statusbar.addWidget(self.file_label, 1)

        self.cursor_label = QLabel("Ln 1, Col 1")
        self.statusbar.addPermanentWidget(self.cursor_label)

    def _apply_settings(self):
        """Apply settings to the window"""
        ws = self.settings.settings.window
        self.resize(ws.width, ws.height)
        if ws.maximized:
            self.showMaximized()

        # Set home directory as default file browser root
        home = str(Path.home())
        self.file_browser.set_root_path(home)
        
        # Apply terminal position from settings
        terminal_position = self.settings.settings.terminal.position
        if terminal_position == "right":
            self._set_terminal_position("right")

    def _get_compat_theme(self):
        """Create a compatibility Theme object for components that don't use dual themes yet"""
        # Import here to avoid circular dependency
        from forge_ide.app.themes import Theme
        ui_theme = self.theme_manager.get_current_ui_theme()
        syntax_theme = self.theme_manager.get_current_syntax_theme()
        
        # Create a Theme object combining UI and syntax themes
        return Theme(
            name=ui_theme.name,
            is_dark=ui_theme.is_dark,
            background=ui_theme.background,
            foreground=ui_theme.foreground,
            accent=ui_theme.accent,
            accent_hover=ui_theme.accent_hover,
            panel_background=ui_theme.panel_background,
            panel_border=ui_theme.panel_border,
            editor_background=ui_theme.editor_background,
            editor_foreground=ui_theme.editor_foreground,
            editor_line_highlight=ui_theme.editor_line_highlight,
            editor_selection=ui_theme.editor_selection,
            editor_gutter_bg=ui_theme.editor_gutter_bg,
            editor_gutter_fg=ui_theme.editor_gutter_fg,
            tab_background=ui_theme.tab_background,
            tab_active_background=ui_theme.tab_active_background,
            tab_hover_background=ui_theme.tab_hover_background,
            tab_border=ui_theme.tab_border,
            browser_background=ui_theme.browser_background,
            browser_item_hover=ui_theme.browser_item_hover,
            browser_item_selected=ui_theme.browser_item_selected,
            terminal_background=ui_theme.terminal_background,
            terminal_foreground=ui_theme.terminal_foreground,
            scrollbar_background=ui_theme.scrollbar_background,
            scrollbar_handle=ui_theme.scrollbar_handle,
            scrollbar_handle_hover=ui_theme.scrollbar_handle_hover,
            button_background=ui_theme.button_background,
            button_foreground=ui_theme.button_foreground,
            button_hover=ui_theme.button_hover,
            button_pressed=ui_theme.button_pressed,
            input_background=ui_theme.input_background,
            input_border=ui_theme.input_border,
            input_focus_border=ui_theme.input_focus_border,
            success=ui_theme.success,
            warning=ui_theme.warning,
            error=ui_theme.error,
            info=ui_theme.info,
            syntax=syntax_theme,
        )
    
    def _apply_theme(self):
        """Apply the current theme to all components"""
        theme = self._get_compat_theme()
        self.file_browser.apply_theme(theme)
        self.terminal.apply_theme(theme)
        self.find_replace.apply_theme(theme)

        # Apply to all open editors (dual theme)
        ui_theme = self.theme_manager.get_current_ui_theme()
        syntax_theme = self.theme_manager.get_current_syntax_theme()
        for editor in self.editors.values():
            editor.apply_ui_theme(ui_theme)
            editor.apply_syntax_theme(syntax_theme)

    def _restore_session(self):
        """Restore previous session state (open files, project folder)"""
        session = self.settings.settings.session

        # Restore project folder
        if session.project_path and Path(session.project_path).exists():
            self.file_browser.set_root_path(session.project_path)
            self.current_project_path = session.project_path

        # Restore open files
        for file_path in session.open_files:
            if Path(file_path).exists():
                self.open_file(file_path)

        # Restore active tab
        if session.active_file and session.active_file in self.editors:
            idx = self.tab_widget.indexOf(self.editors[session.active_file])
            if idx >= 0:
                self.tab_widget.setCurrentIndex(idx)

    def _set_ui_theme(self, name: str):
        """Set and apply a new UI theme"""
        self.theme_manager.set_ui_theme(name)
        from PyQt6.QtWidgets import QApplication
        QApplication.instance().setStyleSheet(self.theme_manager.get_current_stylesheet())
        
        # Check syntax theme compatibility
        ui_theme = self.theme_manager.get_current_ui_theme()
        compatible_names = self.theme_manager.get_compatible_syntax_themes(ui_theme.is_dark)
        current_syntax_name = getattr(self.settings.settings.theme, 'syntax_theme', 'default')
        
        if current_syntax_name not in compatible_names and compatible_names:
            # Switch to first compatible
            new_syntax = compatible_names[0]
            print(f"Switching incompatible syntax theme {current_syntax_name} to {new_syntax}")
            self._set_syntax_theme(new_syntax)
            
        self._update_syntax_theme_menu()
        self._apply_theme()
    
    def _set_syntax_theme(self, name: str):
        """Set and apply a new syntax theme"""
        self.theme_manager.set_syntax_theme(name)
        # Update menu to show new selection
        self._update_syntax_theme_menu()
        
        # Only update editors
        syntax_theme = self.theme_manager.get_current_syntax_theme()
        for editor in self.editors.values():
            editor.apply_syntax_theme(syntax_theme)

    def _update_syntax_theme_menu(self):
        """Update syntax theme menu with compatible themes"""
        if not hasattr(self, 'syntax_theme_menu'):
            return

        self.syntax_theme_menu.clear()
        
        # Get compatible themes for current UI theme
        ui_theme = self.theme_manager.get_current_ui_theme()
        is_dark = ui_theme.is_dark
        compatible_themes = self.theme_manager.get_compatible_syntax_themes(is_dark)
        
        # Add actions
        for theme_name in compatible_themes:
            display_name = theme_name.replace('_', ' ').title()
            
            # Add checkmark if active
            current_syntax = getattr(self.settings.settings.theme, 'syntax_theme', 'default')
            if theme_name == current_syntax:
                display_name = f"✓ {display_name}"
                
            theme_action = QAction(display_name, self)
            theme_action.triggered.connect(lambda checked, n=theme_name: self._set_syntax_theme(n))
            self.syntax_theme_menu.addAction(theme_action)
            
        # Add separator and option to show all themes
        self.syntax_theme_menu.addSeparator()
        show_all_action = QAction("Show All Themes", self)
        # Check if we are currently showing all? 
        # For now, just a button to repopulate with all
        show_all_action.triggered.connect(self._show_all_syntax_themes)
        self.syntax_theme_menu.addAction(show_all_action)

    def _show_all_syntax_themes(self):
        """Show all syntax themes in the menu regardless of compatibility"""
        self.syntax_theme_menu.clear()
        for theme_name in self.theme_manager.get_available_syntax_themes():
            display_name = theme_name.replace('_', ' ').title()
            current_syntax = getattr(self.settings.settings.theme, 'syntax_theme', 'default')
            if theme_name == current_syntax:
                display_name = f"✓ {display_name}"
                
            theme_action = QAction(display_name, self)
            theme_action.triggered.connect(lambda checked, n=theme_name: self._set_syntax_theme(n))
            self.syntax_theme_menu.addAction(theme_action)

    # File operations
    def new_file(self):
        """Create a new empty file"""
        editor = CodeEditor(
            ui_theme=self.theme_manager.get_current_ui_theme(),
            syntax_theme=self.theme_manager.get_current_syntax_theme(),
            settings=self.settings
        )
        editor.cursorPositionChanged.connect(self._update_cursor_position)

        self.tab_widget.addTab(editor, "Untitled")
        self.tab_widget.setCurrentWidget(editor)

    def open_file_dialog(self):
        """Open file dialog"""
        start_dir = self.current_project_path if self.current_project_path else str(Path.home())
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Open File", start_dir,
            "FORGE Files (*.fg);;All Files (*)"
        )
        if file_path:
            self.open_file(file_path)

    def open_folder_dialog(self):
        """Open folder dialog"""
        start_dir = self.current_project_path if self.current_project_path else str(Path.home())
        folder = QFileDialog.getExistingDirectory(
            self, "Open Folder", start_dir
        )
        if folder:
            self.file_browser.set_root_path(folder)
            self.current_project_path = folder

    def open_file(self, file_path: str):
        """Open a file in a new or existing tab"""
        path = Path(file_path)

        # Check if already open
        if file_path in self.editors:
            idx = self.tab_widget.indexOf(self.editors[file_path])
            self.tab_widget.setCurrentIndex(idx)
            return

        if not path.exists():
            QMessageBox.warning(self, "Error", f"File not found: {file_path}")
            return

        try:
            content = path.read_text(encoding='utf-8')
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Could not read file: {e}")
            return

        editor = CodeEditor(
            ui_theme=self.theme_manager.get_current_ui_theme(),
            syntax_theme=self.theme_manager.get_current_syntax_theme(),
            settings=self.settings
        )
        editor.file_path = file_path
        editor.setPlainText(content)
        editor.set_modified(False)
        editor.cursorPositionChanged.connect(self._update_cursor_position)
        editor.file_modified.connect(lambda m, p=file_path: self._on_file_modified(p, m))

        self.editors[file_path] = editor
        idx = self.tab_widget.addTab(editor, path.name)
        self.tab_widget.setCurrentIndex(idx)

        self.settings.add_recent_file(file_path)
        self.file_label.setText(file_path)

    def save_file(self):
        """Save the current file"""
        editor = self.tab_widget.currentWidget()
        if not isinstance(editor, CodeEditor):
            return

        if editor.file_path:
            self._save_editor(editor, editor.file_path)
        else:
            self.save_file_as()

    def save_file_as(self):
        """Save the current file with a new name"""
        current_editor = self.tab_widget.currentWidget()
        if not isinstance(current_editor, CodeEditor):
            return

        # Use suggested save path (e.g. from converter), then editor's own path, then project dir
        initial_path = getattr(current_editor, 'suggested_save_path', None) or current_editor.file_path
        if not initial_path:
            initial_path = self.current_project_path if self.current_project_path else str(Path.home())
        file_path, _ = QFileDialog.getSaveFileName(
            self, "Save File", initial_path,
            "FORGE Files (*.fg);;All Files (*)"
        )
        if file_path:
            # Create a NEW editor with the same content
            new_editor = CodeEditor(
                ui_theme=self.theme_manager.get_current_ui_theme(),
                syntax_theme=self.theme_manager.get_current_syntax_theme(),
                settings=self.settings
            )
            new_editor.setPlainText(current_editor.toPlainText())
            new_editor.cursorPositionChanged.connect(self._update_cursor_position)
            new_editor.file_modified.connect(lambda m, p=file_path: self._on_file_modified(p, m))

            # Save the new editor to the new file
            self._save_editor(new_editor, file_path)

            # Add new editor as a new tab
            self.editors[file_path] = new_editor
            idx = self.tab_widget.addTab(new_editor, Path(file_path).name)
            self.tab_widget.setCurrentIndex(idx)

            # Original editor remains unchanged and associated with its original file

    def _save_editor(self, editor: CodeEditor, file_path: str):
        """Save editor content to file"""
        try:
            Path(file_path).write_text(editor.toPlainText(), encoding='utf-8')
            editor.file_path = file_path
            editor.set_modified(False)

            # Update tab title
            idx = self.tab_widget.indexOf(editor)
            self.tab_widget.setTabText(idx, Path(file_path).name)

            # Update editors dict
            if file_path not in self.editors:
                self.editors[file_path] = editor

            self.file_label.setText(file_path)
            self.statusbar.showMessage("File saved", 3000)
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Could not save file: {e}")

    def close_tab(self, index: int):
        """Close a tab"""
        editor = self.tab_widget.widget(index)
        if not isinstance(editor, CodeEditor):
            return

        if editor.is_modified():
            reply = QMessageBox.question(
                self, "Save Changes?",
                "This file has unsaved changes. Save before closing?",
                QMessageBox.StandardButton.Save |
                QMessageBox.StandardButton.Discard |
                QMessageBox.StandardButton.Cancel
            )

            if reply == QMessageBox.StandardButton.Cancel:
                return
            elif reply == QMessageBox.StandardButton.Save:
                self.tab_widget.setCurrentIndex(index)
                self.save_file()

        # Remove from editors dict
        if editor.file_path and editor.file_path in self.editors:
            del self.editors[editor.file_path]

        self.tab_widget.removeTab(index)

    def _on_tab_changed(self, index: int):
        """Handle tab change"""
        editor = self.tab_widget.widget(index)
        if isinstance(editor, CodeEditor) and editor.file_path:
            self.file_label.setText(editor.file_path)
        else:
            self.file_label.setText("No file open")
        self._update_cursor_position()
        # Update find/replace to target the new editor
        if isinstance(editor, CodeEditor) and self.find_replace.isVisible():
            self.find_replace.set_editor(editor)

    def _on_file_modified(self, path: str, modified: bool):
        """Handle file modification"""
        if path in self.editors:
            editor = self.editors[path]
            idx = self.tab_widget.indexOf(editor)
            name = Path(path).name
            if modified:
                self.tab_widget.setTabText(idx, f"● {name}")
            else:
                self.tab_widget.setTabText(idx, name)

    def _update_cursor_position(self):
        """Update cursor position in status bar"""
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            cursor = editor.textCursor()
            line = cursor.blockNumber() + 1
            col = cursor.columnNumber() + 1
            self.cursor_label.setText(f"Ln {line}, Col {col}")

    # Edit operations
    def _undo(self):
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.undo()

    def _redo(self):
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.redo()

    def _cut(self):
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.cut()

    def _copy(self):
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.copy()

    def _paste(self):
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.paste()

    # View operations
    def _toggle_file_browser(self):
        """Toggle file browser visibility"""
        self.file_browser.setVisible(not self.file_browser.isVisible())

    def _toggle_terminal(self):
        """Toggle terminal visibility"""
        self.terminal.setVisible(not self.terminal.isVisible())
    
    def _set_terminal_position(self, position: str):
        """Set terminal position to 'bottom' or 'right'"""
        if position not in ["bottom", "right"]:
            return

        # Check if terminal is already in the desired position by checking
        # which splitter actually contains it
        terminal_in_middle = any(
            self.middle_splitter.widget(i) is self.terminal
            for i in range(self.middle_splitter.count())
        )
        if position == "bottom" and terminal_in_middle:
            return
        if position == "right" and not terminal_in_middle:
            return

        # Save the new position
        self.settings.settings.terminal.position = position
        self.settings.save()

        # Get current visibility state
        was_visible = self.terminal.isVisible()

        # Hide and detach terminal from its current splitter.
        # addWidget / insertWidget will automatically reparent the widget,
        # so we just need to hide it first to avoid visual glitches.
        self.terminal.hide()

        # Add terminal to new position (reparents automatically)
        if position == "bottom":
            self.middle_splitter.addWidget(self.terminal)
            self.middle_splitter.setSizes([600, 200])
        else:  # position == "right"
            self.main_splitter.addWidget(self.terminal)
            # Adjust sizes: file_browser | middle | terminal
            self.main_splitter.setSizes([250, 600, 400])

        # Restore visibility
        self.terminal.setVisible(was_visible)

    def _on_bookmark_navigated(self, path: str):
        """Handle bookmark navigation - update project path"""
        self.current_project_path = path

    # Find/Replace operations
    def _show_find(self):
        """Show the find bar"""
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            self.find_replace.set_editor(editor)
            self.find_replace.show_find()

    def _show_replace(self):
        """Show the find and replace bar"""
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            self.find_replace.set_editor(editor)
            self.find_replace.show_find_replace()
    
    def _indent_selection(self):
        """Indent the selected lines in the current editor"""
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.indent_selection()
    
    def _dedent_selection(self):
        """Dedent the selected lines in the current editor"""
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.dedent_selection()
    
    def _comment_selection(self):
        """Toggle # comment on the selected lines"""
        editor = self.tab_widget.currentWidget()
        if isinstance(editor, CodeEditor):
            editor.comment_selection()

    def _show_preferences(self):
        """Show the preferences/settings dialog"""
        dialog = SettingsDialog(self.settings, self.theme_manager, parent=self)

        # Connect signal to apply settings immediately when Apply/OK is clicked
        dialog.settings_applied.connect(self._apply_preferences_changes)

        dialog.exec()

    def _apply_preferences_changes(self):
        """Apply preference changes to all editors and components"""
        # Apply changed settings to all editors
        for editor in self.editors.values():
            editor.apply_settings()

        # Apply terminal settings
        self.terminal.apply_settings()

        # Apply theme if changed
        ui_theme_name = getattr(self.settings.settings.theme, 'ui_theme', 'dark')
        syntax_theme_name = getattr(self.settings.settings.theme, 'syntax_theme', 'default')
        
        # Get the currently cached theme name from theme manager (not the UITheme.name property)
        # The theme manager stores themes with lowercase keys, so we compare those
        current_ui_theme_key = getattr(self.settings.settings.theme, 'ui_theme', 'dark')
        
        # Always apply the theme when preferences change to ensure UI updates
        self._set_ui_theme(ui_theme_name)
        
        # Check if syntax theme changed by name (syntax themes are loaded from files)
        if syntax_theme_name not in self.theme_manager.get_available_syntax_themes():
            # Theme was removed, reset to default
            self._set_syntax_theme('default')
        else:
            self._set_syntax_theme(syntax_theme_name)

    # Run operations
    def run_current_file(self):
        """Run the current FORGE file"""
        editor = self.tab_widget.currentWidget()
        if not isinstance(editor, CodeEditor):
            QMessageBox.warning(self, "No File", "No file is open to run.")
            return

        # Save first if modified
        if editor.is_modified():
            if editor.file_path:
                self._save_editor(editor, editor.file_path)
            else:
                reply = QMessageBox.question(
                    self, "Save File?",
                    "File must be saved before running. Save now?",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                )
                if reply == QMessageBox.StandardButton.Yes:
                    self.save_file_as()
                else:
                    return

        if not editor.file_path:
            QMessageBox.warning(self, "No File", "Please save the file first.")
            return

        # Make sure terminal is visible
        self.terminal.setVisible(True)

        # Run the file
        self.terminal.run_forge_file(editor.file_path)

    def run_current_file_external(self):
        """Run the current FORGE file in an external terminal"""
        editor = self.tab_widget.currentWidget()
        if not isinstance(editor, CodeEditor):
            QMessageBox.warning(self, "No File", "No file is open to run.")
            return

        # Save first if modified
        if editor.is_modified():
            if editor.file_path:
                self._save_editor(editor, editor.file_path)
                # If still modified (save failed), abort
                if editor.is_modified():
                    return
            else:
                reply = QMessageBox.question(
                    self, "Save File?",
                    "File must be saved before running. Save now?",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                )
                if reply == QMessageBox.StandardButton.Yes:
                    self.save_file_as()
                    # If still no path (cancelled), abort
                    if not editor.file_path:
                        return
                else:
                    return

        if not editor.file_path:
            QMessageBox.warning(self, "No File", "Please save the file first.")
            return

        # Run the file externally
        self.terminal.run_external_file(editor.file_path)

    def _show_interpreter_settings(self):
        """Open settings dialog directly to the Runtime tab"""
        dialog = SettingsDialog(self.settings, self.theme_manager, parent=self)
        dialog.settings_applied.connect(self._apply_preferences_changes)
        
        # Switch to Runtime tab (index 4)
        # 0=Editor, 1=Theme, 2=Terminal, 3=Shortcuts, 4=Runtime
        dialog.tabs.setCurrentIndex(4)
        
        dialog.exec()

    def _show_quick_reference(self):
        """Show the FORGE quick reference help viewer"""
        viewer = HelpViewer(parent=self, theme=self._get_compat_theme())
        viewer.apply_theme(self.theme_manager.get_current_theme())
        viewer.exec()

    def _show_about(self):
        """Show about dialog"""
        QMessageBox.about(
            self, "About FORGE IDE",
            "<h2>FORGE IDE</h2>"
            "<p>Version 1.0.0</p>"
            "<p>A modern IDE for the FORGE programming language.</p>"
            "<p>(c) 2026 Chuck Finch - Fragillidae Software</p>"
        )

    def closeEvent(self, event):
        """Handle window close"""
        # Check for unsaved files
        for editor in self.editors.values():
            if editor.is_modified():
                reply = QMessageBox.question(
                    self, "Unsaved Changes",
                    "You have unsaved changes. Are you sure you want to quit?",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                )
                if reply == QMessageBox.StandardButton.No:
                    event.ignore()
                    return
                break

        # Save window state
        ws = self.settings.settings.window
        ws.width = self.width()
        ws.height = self.height()
        ws.maximized = self.isMaximized()

        # Save session state
        session = self.settings.settings.session
        session.open_files = [
            path for path, editor in self.editors.items()
            if path and Path(path).exists()
        ]
        current_editor = self.tab_widget.currentWidget()
        if isinstance(current_editor, CodeEditor) and current_editor.file_path:
            session.active_file = current_editor.file_path
        else:
            session.active_file = ""
        session.project_path = self.current_project_path or ""

        self.settings.save()

        event.accept()






