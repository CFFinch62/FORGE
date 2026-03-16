"""
Settings Manager for FORGE IDE
Handles loading, saving, and managing user preferences
"""

import json
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import List, Optional


def get_config_dir() -> Path:
    """Get the configuration directory for FORGE IDE"""
    config_dir = Path.home() / ".config" / "forge_ide"
    config_dir.mkdir(parents=True, exist_ok=True)
    return config_dir


@dataclass
class EditorSettings:
    """Editor-specific settings"""
    font_family: str = "JetBrains Mono"
    font_size: int = 12
    tab_width: int = 4
    use_spaces: bool = True
    show_line_numbers: bool = True
    word_wrap: bool = False
    highlight_current_line: bool = True
    bracket_matching: bool = True


@dataclass
class FileBrowserSettings:
    """File browser settings"""
    show_hidden_files: bool = False
    bookmarks: List[str] = field(default_factory=list)
    last_directory: str = ""


@dataclass
class ThemeSettings:
    """Theme-related settings"""
    ui_theme: str = "dark"          # UI theme name
    syntax_theme: str = "default"   # Syntax theme name (from .conf file)
    # Legacy field for migration
    current_theme: str = "dark"


@dataclass
class WindowSettings:
    """Window state settings"""
    width: int = 1200
    height: int = 800
    maximized: bool = False
    file_browser_visible: bool = True
    file_browser_width: int = 250
    terminal_height: int = 200
    terminal_width: int = 400


@dataclass
class TerminalSettings:
    """Terminal panel settings"""
    font_family: str = "JetBrains Mono"
    font_size: int = 11
    visible: bool = True
    position: str = "bottom"  # "bottom" or "right"
    external_terminal_command: str = ""  # Command template for external terminal


@dataclass
class SessionSettings:
    """Session state for persistence across restarts"""
    open_files: List[str] = field(default_factory=list)
    active_file: str = ""
    project_path: str = ""


@dataclass
class Settings:
    """All IDE settings"""
    editor: EditorSettings = field(default_factory=EditorSettings)
    file_browser: FileBrowserSettings = field(default_factory=FileBrowserSettings)
    theme: ThemeSettings = field(default_factory=ThemeSettings)
    window: WindowSettings = field(default_factory=WindowSettings)
    terminal: TerminalSettings = field(default_factory=TerminalSettings)
    forge_interpreter_path: str = ""  # Path to FORGE interpreter (auto-detected if empty)
    recent_files: List[str] = field(default_factory=list)
    session: SessionSettings = field(default_factory=SessionSettings)


class SettingsManager:
    """Manages loading and saving of settings"""
    
    def __init__(self):
        self.config_file = get_config_dir() / "settings.json"
        self.settings = self._load()
    
    def _load(self) -> Settings:
        """Load settings from file or create defaults"""
        if self.config_file.exists():
            try:
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    return self._dict_to_settings(data)
            except Exception as e:
                print(f"Warning: Could not load settings: {e}")
        return Settings()
    
    def _dict_to_settings(self, data: dict) -> Settings:
        """Convert dictionary to Settings object"""
        settings = Settings()
        
        if 'editor' in data:
            settings.editor = EditorSettings(**data['editor'])
        if 'file_browser' in data:
            fb = data['file_browser']
            settings.file_browser = FileBrowserSettings(
                show_hidden_files=fb.get('show_hidden_files', False),
                bookmarks=fb.get('bookmarks', []),
                last_directory=fb.get('last_directory', ''),
            )
        elif 'bookmarks' in data:
            # Migrate legacy flat bookmarks list
            settings.file_browser.bookmarks = data['bookmarks']
        if 'theme' in data:
            settings.theme = ThemeSettings(**data['theme'])
        if 'window' in data:
            settings.window = WindowSettings(**data['window'])
        if 'terminal' in data:
            settings.terminal = TerminalSettings(**data['terminal'])
        if 'forge_interpreter_path' in data:
            settings.forge_interpreter_path = data['forge_interpreter_path']
        if 'recent_files' in data:
            settings.recent_files = data['recent_files']
        if 'session' in data:
            settings.session = SessionSettings(**data['session'])

        return settings
    
    def save(self):
        """Save settings to file"""
        data = {
            'editor': asdict(self.settings.editor),
            'file_browser': asdict(self.settings.file_browser),
            'theme': asdict(self.settings.theme),
            'window': asdict(self.settings.window),
            'terminal': asdict(self.settings.terminal),
            'forge_interpreter_path': self.settings.forge_interpreter_path,
            'recent_files': self.settings.recent_files[:20],  # Keep last 20
            'session': asdict(self.settings.session),
        }
        
        try:
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            print(f"Warning: Could not save settings: {e}")
    
    def add_recent_file(self, filepath: str):
        """Add a file to the recent files list"""
        if filepath in self.settings.recent_files:
            self.settings.recent_files.remove(filepath)
        self.settings.recent_files.insert(0, filepath)
        self.settings.recent_files = self.settings.recent_files[:20]
        self.save()

    def add_bookmark(self, path: str):
        """Add a folder to bookmarks"""
        if path not in self.settings.file_browser.bookmarks:
            self.settings.file_browser.bookmarks.append(path)
            self.save()

    def remove_bookmark(self, path: str):
        """Remove a folder from bookmarks"""
        if path in self.settings.file_browser.bookmarks:
            self.settings.file_browser.bookmarks.remove(path)
            self.save()

