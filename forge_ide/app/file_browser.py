"""
File Browser Widget for FORGE IDE
Provides a file tree view for project navigation with toolbar, path bar, bookmarks,
and navigation history - matching the experience of the STEPS IDE.
"""

import os
import shutil
from pathlib import Path
from typing import List, Optional

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTreeView,
    QPushButton, QLineEdit, QMenu, QMessageBox, QInputDialog,
    QToolButton, QFrame, QLabel, QListWidget, QListWidgetItem,
    QSplitter, QHeaderView, QAbstractItemView, QToolBar
)
from PyQt6.QtCore import Qt, QDir, QModelIndex, pyqtSignal, QSize, QTimer
from PyQt6.QtGui import QFileSystemModel, QAction

from forge_ide.app.themes import Theme
from forge_ide.app.settings import SettingsManager


class FileBrowserWidget(QWidget):
    """File browser widget with toolbar, path bar, bookmarks, navigation history and tree view"""

    file_double_clicked = pyqtSignal(str)   # Emits file path on double-click / open
    bookmark_navigated = pyqtSignal(str)    # Emits folder path when bookmark clicked

    def __init__(self, parent=None, theme: Theme = None, settings: SettingsManager = None):
        super().__init__(parent)
        self.theme = theme
        self.settings = settings
        self._current_root = str(Path.home())

        # Click disambiguation timer (single vs double click on folders)
        self._click_timer = QTimer(self)
        self._click_timer.setSingleShot(True)
        self._click_timer.timeout.connect(self._on_single_click_timeout)
        self._pending_click_index = None

        # Navigation history
        self._history: List[str] = []
        self._history_index = -1

        self._setup_ui()
        self._setup_connections()
        self._load_initial_directory()

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------

    def _setup_ui(self):
        """Set up the file browser UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # --- Toolbar ---
        self.toolbar = QToolBar()
        self.toolbar.setIconSize(QSize(16, 16))
        self.toolbar.setMovable(False)

        self.back_btn = QToolButton()
        self.back_btn.setText("←")
        self.back_btn.setToolTip("Go Back")
        self.toolbar.addWidget(self.back_btn)

        self.forward_btn = QToolButton()
        self.forward_btn.setText("→")
        self.forward_btn.setToolTip("Go Forward")
        self.toolbar.addWidget(self.forward_btn)

        self.up_btn = QToolButton()
        self.up_btn.setText("↑")
        self.up_btn.setToolTip("Go Up")
        self.toolbar.addWidget(self.up_btn)

        self.home_btn = QToolButton()
        self.home_btn.setText("⌂")
        self.home_btn.setToolTip("Go Home")
        self.toolbar.addWidget(self.home_btn)

        self.toolbar.addSeparator()

        self.refresh_btn = QToolButton()
        self.refresh_btn.setText("⟳")
        self.refresh_btn.setToolTip("Refresh")
        self.toolbar.addWidget(self.refresh_btn)

        self.toolbar.addSeparator()

        self.bookmark_btn = QToolButton()
        self.bookmark_btn.setText("⭐")
        self.bookmark_btn.setToolTip("Bookmark Current Folder")
        self.toolbar.addWidget(self.bookmark_btn)

        self.hidden_toggle = QToolButton()
        self.hidden_toggle.setText("👁")
        self.hidden_toggle.setToolTip("Toggle Hidden Files")
        self.hidden_toggle.setCheckable(True)
        if self.settings:
            self.hidden_toggle.setChecked(
                self.settings.settings.file_browser.show_hidden_files
            )
        self.toolbar.addWidget(self.hidden_toggle)

        layout.addWidget(self.toolbar)

        # --- Path bar ---
        self.path_bar = QLineEdit()
        self.path_bar.setPlaceholderText("Enter path…")
        layout.addWidget(self.path_bar)

        # --- Splitter: bookmarks above, tree below ---
        splitter = QSplitter(Qt.Orientation.Vertical)

        # Bookmarks section
        bookmarks_container = QWidget()
        bm_layout = QVBoxLayout(bookmarks_container)
        bm_layout.setContentsMargins(0, 0, 0, 0)
        bm_layout.setSpacing(0)

        bm_header_layout = QHBoxLayout()
        bm_header_layout.setContentsMargins(8, 6, 8, 2)

        bm_label = QLabel("Bookmarks")
        bm_label.setStyleSheet("font-weight: bold; font-size: 11px;")
        bm_header_layout.addWidget(bm_label)
        bm_header_layout.addStretch()
        bm_layout.addLayout(bm_header_layout)

        self.bookmark_list = QListWidget()
        self.bookmark_list.setMaximumHeight(120)
        self.bookmark_list.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.bookmark_list.customContextMenuRequested.connect(
            self._show_bookmark_context_menu
        )
        self.bookmark_list.itemClicked.connect(self._on_bookmark_clicked)
        bm_layout.addWidget(self.bookmark_list)

        splitter.addWidget(bookmarks_container)

        # File tree section
        tree_container = QWidget()
        tree_layout = QVBoxLayout(tree_container)
        tree_layout.setContentsMargins(0, 0, 0, 0)
        tree_layout.setSpacing(0)

        # Tree header (shows current folder name)
        tree_header = QFrame()
        tree_header_layout = QHBoxLayout(tree_header)
        tree_header_layout.setContentsMargins(8, 4, 8, 4)

        self.folder_label = QLabel("Files")
        self.folder_label.setStyleSheet("font-weight: bold; font-size: 11px;")
        tree_header_layout.addWidget(self.folder_label)
        tree_header_layout.addStretch()
        tree_layout.addWidget(tree_header)

        # File system model
        self.model = QFileSystemModel()
        self.model.setReadOnly(False)
        self._update_filters()

        # Tree view
        self.tree_view = QTreeView()
        self.tree_view.setModel(self.model)
        self.tree_view.setHeaderHidden(True)
        self.tree_view.setAnimated(True)
        self.tree_view.setIndentation(16)
        self.tree_view.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.tree_view.customContextMenuRequested.connect(self._show_context_menu)
        self.tree_view.clicked.connect(self._on_item_clicked)
        self.tree_view.doubleClicked.connect(self._on_double_click)

        # Hide size, type, date columns
        self.tree_view.setColumnHidden(1, True)
        self.tree_view.setColumnHidden(2, True)
        self.tree_view.setColumnHidden(3, True)

        tree_layout.addWidget(self.tree_view)
        splitter.addWidget(tree_container)
        splitter.setSizes([120, 400])

        layout.addWidget(splitter)

    def _setup_connections(self):
        """Wire up signals"""
        self.back_btn.clicked.connect(self._go_back)
        self.forward_btn.clicked.connect(self._go_forward)
        self.up_btn.clicked.connect(self._go_up)
        self.home_btn.clicked.connect(self._go_home)
        self.refresh_btn.clicked.connect(self._refresh)
        self.bookmark_btn.clicked.connect(self._bookmark_current_folder)
        self.hidden_toggle.toggled.connect(self._toggle_hidden_files)
        self.path_bar.returnPressed.connect(self._on_path_entered)

    # ------------------------------------------------------------------
    # Navigation
    # ------------------------------------------------------------------

    def _load_initial_directory(self):
        """Navigate to last-used directory (from settings) or home"""
        start = str(Path.home())
        if self.settings:
            last = self.settings.settings.file_browser.last_directory
            if last and os.path.exists(last):
                start = last
        self.navigate_to(start)
        self._load_bookmarks()

    def _update_filters(self):
        """Update file-system model filters based on hidden-files setting"""
        show_hidden = (
            self.settings.settings.file_browser.show_hidden_files
            if self.settings
            else False
        )
        if show_hidden:
            self.model.setFilter(
                QDir.Filter.AllEntries | QDir.Filter.NoDotAndDotDot |
                QDir.Filter.Hidden | QDir.Filter.AllDirs
            )
        else:
            self.model.setFilter(
                QDir.Filter.AllEntries | QDir.Filter.NoDotAndDotDot |
                QDir.Filter.AllDirs
            )

    def navigate_to(self, path: str, add_to_history: bool = True):
        """Navigate the file browser to *path* (a directory)."""
        path = os.path.abspath(path)
        if not os.path.exists(path):
            return
        if os.path.isfile(path):
            path = os.path.dirname(path)

        self._current_root = path
        self.model.setRootPath(path)
        self.tree_view.setRootIndex(self.model.index(path))
        self.path_bar.setText(path)
        self.folder_label.setText(os.path.basename(path) or path)

        # History management
        if add_to_history:
            if self._history_index < len(self._history) - 1:
                self._history = self._history[: self._history_index + 1]
            self._history.append(path)
            self._history_index = len(self._history) - 1

        self.back_btn.setEnabled(self._history_index > 0)
        self.forward_btn.setEnabled(self._history_index < len(self._history) - 1)

        # Persist last directory
        if self.settings:
            self.settings.settings.file_browser.last_directory = path
            self.settings.save()

    def set_root_path(self, path: str):
        """Alias for navigate_to() — preserves compatibility with main_window.py calls."""
        self.navigate_to(path)

    # Toolbar button handlers

    def _go_back(self):
        if self._history_index > 0:
            self._history_index -= 1
            self.navigate_to(self._history[self._history_index], add_to_history=False)

    def _go_forward(self):
        if self._history_index < len(self._history) - 1:
            self._history_index += 1
            self.navigate_to(self._history[self._history_index], add_to_history=False)

    def _go_up(self):
        if self._current_root:
            parent = os.path.dirname(self._current_root)
            if parent and parent != self._current_root:
                self.navigate_to(parent)

    def _go_home(self):
        self.navigate_to(str(Path.home()))

    def _refresh(self):
        if self._current_root:
            self.model.setRootPath("")
            self.model.setRootPath(self._current_root)
            self.tree_view.setRootIndex(self.model.index(self._current_root))

    def _bookmark_current_folder(self):
        if self._current_root:
            self.add_bookmark(self._current_root)

    def _toggle_hidden_files(self, show: bool):
        if self.settings:
            self.settings.settings.file_browser.show_hidden_files = show
            self.settings.save()
        self._update_filters()
        self._refresh()

    def _on_path_entered(self):
        path = self.path_bar.text()
        if os.path.exists(path):
            if os.path.isfile(path):
                self.file_double_clicked.emit(path)
            else:
                self.navigate_to(path)
        else:
            QMessageBox.warning(self, "Invalid Path", f"Path does not exist: {path}")

    # ------------------------------------------------------------------
    # Tree clicks
    # ------------------------------------------------------------------

    def _on_item_clicked(self, index: QModelIndex):
        path = self.model.filePath(index)
        if os.path.isdir(path):
            self._pending_click_index = index
            self._click_timer.start(250)

    def _on_single_click_timeout(self):
        if self._pending_click_index is not None:
            index = self._pending_click_index
            self._pending_click_index = None
            path = self.model.filePath(index)
            if os.path.isdir(path):
                if self.tree_view.isExpanded(index):
                    self.tree_view.collapse(index)
                else:
                    self.tree_view.expand(index)

    def _on_double_click(self, index: QModelIndex):
        """Handle double-click: navigate into folders, open files."""
        self._click_timer.stop()
        self._pending_click_index = None

        path = self.model.filePath(index)
        if os.path.isdir(path):
            self.navigate_to(path)
        elif os.path.isfile(path):
            self.file_double_clicked.emit(path)

    # ------------------------------------------------------------------
    # Bookmarks
    # ------------------------------------------------------------------

    def _load_bookmarks(self):
        """Populate bookmark list from settings"""
        self.bookmark_list.clear()
        if self.settings:
            for path in self.settings.settings.file_browser.bookmarks:
                self._add_bookmark_item(path)

    def _add_bookmark_item(self, path: str):
        folder_name = Path(path).name or path
        item = QListWidgetItem(f"  {folder_name}")
        item.setData(Qt.ItemDataRole.UserRole, path)
        item.setToolTip(path)
        self.bookmark_list.addItem(item)

    def add_bookmark(self, path: str):
        """Add a folder path to bookmarks"""
        if self.settings:
            if path not in self.settings.settings.file_browser.bookmarks:
                self.settings.add_bookmark(path)
                self._add_bookmark_item(path)

    def remove_bookmark(self, path: str):
        """Remove a folder path from bookmarks"""
        if self.settings:
            self.settings.remove_bookmark(path)
            for i in range(self.bookmark_list.count()):
                item = self.bookmark_list.item(i)
                if item.data(Qt.ItemDataRole.UserRole) == path:
                    self.bookmark_list.takeItem(i)
                    break

    def _on_bookmark_clicked(self, item: QListWidgetItem):
        path = item.data(Qt.ItemDataRole.UserRole)
        if path and Path(path).exists():
            self.navigate_to(path)
            self.bookmark_navigated.emit(path)

    def _show_bookmark_context_menu(self, position):
        item = self.bookmark_list.itemAt(position)
        if not item:
            return
        path = item.data(Qt.ItemDataRole.UserRole)
        menu = QMenu(self)

        open_action = QAction("Open", self)
        open_action.triggered.connect(lambda: self._on_bookmark_clicked(item))
        menu.addAction(open_action)

        menu.addSeparator()

        remove_action = QAction("Remove Bookmark", self)
        remove_action.triggered.connect(lambda: self.remove_bookmark(path))
        menu.addAction(remove_action)

        menu.exec(self.bookmark_list.mapToGlobal(position))

    # ------------------------------------------------------------------
    # Context menu & file operations
    # ------------------------------------------------------------------

    def _show_context_menu(self, position):
        index = self.tree_view.indexAt(position)
        path = ""
        target_dir = self._current_root

        if index.isValid():
            path = self.model.filePath(index)
            if os.path.isdir(path):
                target_dir = path
            elif os.path.isfile(path):
                target_dir = str(Path(path).parent)

        menu = QMenu(self)

        new_file_action = QAction("New File…", self)
        new_file_action.triggered.connect(lambda: self._create_new_file(target_dir))
        menu.addAction(new_file_action)

        new_folder_action = QAction("New Folder…", self)
        new_folder_action.triggered.connect(lambda: self._create_new_folder(target_dir))
        menu.addAction(new_folder_action)

        menu.addSeparator()

        if index.isValid():
            if os.path.isfile(path):
                open_action = QAction("Open", self)
                open_action.triggered.connect(lambda: self.file_double_clicked.emit(path))
                menu.addAction(open_action)

            if os.path.isdir(path):
                open_folder_action = QAction("Open Folder", self)
                open_folder_action.triggered.connect(lambda: self.navigate_to(path))
                menu.addAction(open_folder_action)

                bm_list = (
                    self.settings.settings.file_browser.bookmarks
                    if self.settings else []
                )
                if path in bm_list:
                    bm_action = QAction("Remove Bookmark", self)
                    bm_action.triggered.connect(lambda: self.remove_bookmark(path))
                else:
                    bm_action = QAction("Add to Bookmarks", self)
                    bm_action.triggered.connect(lambda: self.add_bookmark(path))
                menu.addAction(bm_action)

            menu.addSeparator()

            rename_action = QAction("Rename…", self)
            rename_action.triggered.connect(lambda: self._rename_item(path))
            menu.addAction(rename_action)

            delete_action = QAction("Delete", self)
            delete_action.triggered.connect(lambda: self._delete_item(path))
            menu.addAction(delete_action)

            menu.addSeparator()

            copy_path_action = QAction("Copy Path", self)
            copy_path_action.triggered.connect(lambda: self._copy_to_clipboard(path))
            menu.addAction(copy_path_action)

        menu.addSeparator()

        if path:
            reveal_action = QAction("Reveal in File Manager", self)
            reveal_action.triggered.connect(lambda: self._reveal_in_file_manager(path))
            menu.addAction(reveal_action)

        refresh_action = QAction("Refresh", self)
        refresh_action.triggered.connect(self._refresh)
        menu.addAction(refresh_action)

        menu.exec(self.tree_view.mapToGlobal(position))

    def _create_new_file(self, directory: str = None):
        directory = directory or self._current_root
        filename, ok = QInputDialog.getText(
            self, "New File", "Enter filename:", text="untitled.fg"
        )
        if ok and filename:
            new_path = Path(directory) / filename
            if new_path.exists():
                QMessageBox.warning(self, "Error", "File already exists!")
                return
            try:
                new_path.touch()
                self.file_double_clicked.emit(str(new_path))
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not create file: {e}")

    def _create_new_folder(self, directory: str = None):
        directory = directory or self._current_root
        foldername, ok = QInputDialog.getText(
            self, "New Folder", "Enter folder name:", text="New Folder"
        )
        if ok and foldername:
            new_path = Path(directory) / foldername
            if new_path.exists():
                QMessageBox.warning(self, "Error", "Folder already exists!")
                return
            try:
                new_path.mkdir()
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not create folder: {e}")

    def _rename_item(self, path: str):
        old_name = os.path.basename(path)
        new_name, ok = QInputDialog.getText(
            self, "Rename", "Enter new name:", text=old_name
        )
        if ok and new_name and new_name != old_name:
            new_path = os.path.join(os.path.dirname(path), new_name)
            try:
                os.rename(path, new_path)
                self._refresh()
            except OSError as e:
                QMessageBox.critical(self, "Error", f"Could not rename: {e}")

    def _delete_item(self, path: str):
        path_obj = Path(path)
        if not path_obj.exists():
            return
        item_type = "folder" if path_obj.is_dir() else "file"
        reply = QMessageBox.question(
            self, f"Delete {item_type.capitalize()}",
            f"Are you sure you want to delete this {item_type}?\n{path}",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        if reply == QMessageBox.StandardButton.Yes:
            try:
                if path_obj.is_dir():
                    shutil.rmtree(path)
                else:
                    path_obj.unlink()
                self._refresh()
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not delete {item_type}: {e}")

    def _copy_to_clipboard(self, text: str):
        from PyQt6.QtWidgets import QApplication
        QApplication.clipboard().setText(text)

    def _reveal_in_file_manager(self, path: str):
        import subprocess, sys
        p = Path(path)
        if p.is_file():
            p = p.parent
        if sys.platform == "linux":
            subprocess.run(["xdg-open", str(p)])
        elif sys.platform == "darwin":
            subprocess.run(["open", str(p)])
        elif sys.platform == "win32":
            subprocess.run(["explorer", str(p)])

    # ------------------------------------------------------------------
    # Theme
    # ------------------------------------------------------------------

    def apply_theme(self, theme: Theme):
        """Apply theme to file browser"""
        self.theme = theme
        self.setStyleSheet(f"""
            QToolBar {{
                background-color: {theme.browser_background};
                border: none;
                spacing: 2px;
                padding: 2px;
            }}
            QToolButton {{
                background-color: transparent;
                color: {theme.foreground};
                border-radius: 3px;
                padding: 2px 4px;
                font-size: 14px;
            }}
            QToolButton:hover {{
                background-color: {theme.browser_item_hover};
            }}
            QToolButton:checked {{
                background-color: {theme.browser_item_selected};
            }}
            QToolButton:disabled {{
                color: {theme.foreground}55;
            }}
            QLineEdit {{
                background-color: {theme.browser_background};
                color: {theme.foreground};
                border: none;
                border-bottom: 1px solid {theme.panel_border};
                padding: 3px 6px;
                font-size: 11px;
            }}
            QTreeView {{
                background-color: {theme.browser_background};
                color: {theme.foreground};
                border: none;
            }}
            QTreeView::item {{
                padding: 4px 8px;
            }}
            QTreeView::item:hover {{
                background-color: {theme.browser_item_hover};
            }}
            QTreeView::item:selected {{
                background-color: {theme.browser_item_selected};
            }}
            QListWidget {{
                background-color: {theme.browser_background};
                color: {theme.foreground};
                border: none;
                font-size: 12px;
            }}
            QListWidget::item {{
                padding: 3px 6px;
                border-radius: 3px;
            }}
            QListWidget::item:hover {{
                background-color: {theme.browser_item_hover};
            }}
            QListWidget::item:selected {{
                background-color: {theme.browser_item_selected};
            }}
            QLabel {{
                color: {theme.foreground};
            }}
            QPushButton {{
                background-color: {theme.button_background};
                color: {theme.button_foreground};
                border: none;
                border-radius: 4px;
                font-weight: bold;
                font-size: 14px;
            }}
            QPushButton:hover {{
                background-color: {theme.button_hover};
            }}
            QFrame {{
                background-color: {theme.panel_border};
            }}
        """)
