"""
Theme Manager for FORGE IDE
Handles loading and applying UI themes and syntax themes separately
"""

import json
from pathlib import Path
from typing import Dict, Any, List, Optional
from dataclasses import dataclass, field
from configparser import ConfigParser


from forge_ide.app.settings import SettingsManager, get_config_dir


@dataclass
class SyntaxColors:
    """Syntax highlighting colors for code editor"""
    keyword: str = "#c678dd"
    builtin: str = "#e5c07b"
    type: str = "#56b6c2"
    string: str = "#98c379"
    number: str = "#d19a66"
    comment: str = "#5c6370"
    operator: str = "#56b6c2"
    function: str = "#61afef"
    variable: str = "#e06c75"
    constant: str = "#d19a66"
    identifier: str = "#abb2bf"
    interpolation: str = "#c678dd"  # For v"..." strings
    theme_type: str = "any"  # "dark", "light", "any"

def _hex_to_rgb(hex_color: str):
    """Convert hex color to (r, g, b) tuple"""
    hex_color = hex_color.lstrip('#')
    return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))

def _get_luminance(hex_color: str) -> float:
    """Calculate relative luminance of a color"""
    try:
        r, g, b = _hex_to_rgb(hex_color)
        return (0.299 * r + 0.587 * g + 0.114 * b) / 255
    except:
        return 0.5

def _determine_theme_type(colors: SyntaxColors) -> str:
    """Determine if a syntax theme is for dark or light backgrounds based on text brightness"""
    # Calculate average luminance of key text colors
    # High luminance text -> meant for Dark background
    # Low luminance text -> meant for Light background
    
    key_colors = [
        colors.identifier,
        colors.keyword,
        colors.function,
        colors.string,
        colors.number
    ]
    
    try:
        total_lum = sum(_get_luminance(c) for c in key_colors)
        avg_lum = total_lum / len(key_colors)
    except:
        return "any"
    
    # Threshold: > 0.5 implies bright text (for dark bg), < 0.5 implies dark text (for light bg)
    return "dark" if avg_lum > 0.5 else "light"


@dataclass
class Theme:
    """Legacy Theme class for backward compatibility with components not yet updated"""
    name: str
    is_dark: bool
    
    # Main colors
    background: str
    foreground: str
    accent: str
    accent_hover: str
    
    # Panel colors
    panel_background: str
    panel_border: str
    
    # Editor colors
    editor_background: str
    editor_foreground: str
    editor_line_highlight: str
    editor_selection: str
    editor_gutter_bg: str
    editor_gutter_fg: str
    
    # Tab colors
    tab_background: str
    tab_active_background: str
    tab_hover_background: str
    tab_border: str
    
    # File browser colors
    browser_background: str
    browser_item_hover: str
    browser_item_selected: str
    
    # Terminal colors
    terminal_background: str
    terminal_foreground: str
    
    # Scrollbar
    scrollbar_background: str
    scrollbar_handle: str
    scrollbar_handle_hover: str
    
    # Button colors
    button_background: str
    button_foreground: str
    button_hover: str
    button_pressed: str
    
    # Input colors
    input_background: str
    input_border: str
    input_focus_border: str
    
    # Status colors
    success: str
    warning: str
    error: str
    info: str
    
    # Syntax highlighting
    syntax: SyntaxColors = None
    
    def __post_init__(self):
        if self.syntax is None:
            self.syntax = SyntaxColors()


@dataclass  
class UITheme:
    """UI theme definition (panels, buttons, tabs, etc.)"""
    name: str
    is_dark: bool
    
    # Main colors
    background: str
    foreground: str
    accent: str
    accent_hover: str
    
    # Panel colors
    panel_background: str
    panel_border: str
    
    # Editor colors (non-syntax)
    editor_background: str
    editor_foreground: str
    editor_line_highlight: str
    editor_selection: str
    editor_gutter_bg: str
    editor_gutter_fg: str
    
    # Tab colors
    tab_background: str
    tab_active_background: str
    tab_hover_background: str
    tab_border: str
    
    # File browser colors
    browser_background: str
    browser_item_hover: str
    browser_item_selected: str
    
    # Terminal colors
    terminal_background: str
    terminal_foreground: str
    
    # Scrollbar
    scrollbar_background: str
    scrollbar_handle: str
    scrollbar_handle_hover: str
    
    # Button colors
    button_background: str
    button_foreground: str
    button_hover: str
    button_pressed: str
    
    # Input colors
    input_background: str
    input_border: str
    input_focus_border: str
    
    # Status colors
    success: str
    warning: str
    error: str
    info: str

    # Colors for nested scope boxes, indexed by nesting depth (cycles if
    # nesting goes deeper than the list). Plain RGB hex, no alpha -- the
    # editor applies a shared alpha value at paint time so all themes stay
    # consistent in how "solid" the boxes look.
    scope_depth_colors: List[str] = field(default_factory=lambda: [
        "#f9c74f", "#577590", "#90be6d", "#9b5de5",
    ])


# Built-in UI Themes
DARK_UI_THEME = UITheme(
    name="Dark",
    is_dark=True,
    background="#1e1e2e",
    foreground="#cdd6f4",
    accent="#89b4fa",
    accent_hover="#74c7ec",
    panel_background="#181825",
    panel_border="#313244",
    editor_background="#1e1e2e",
    editor_foreground="#cdd6f4",
    editor_line_highlight="#2a2a3c",
    editor_selection="#44475a",
    editor_gutter_bg="#181825",
    editor_gutter_fg="#6c7086",
    tab_background="#181825",
    tab_active_background="#1e1e2e",
    tab_hover_background="#313244",
    tab_border="#313244",
    browser_background="#181825",
    browser_item_hover="#313244",
    browser_item_selected="#45475a",
    terminal_background="#11111b",
    terminal_foreground="#cdd6f4",
    scrollbar_background="#181825",
    scrollbar_handle="#45475a",
    scrollbar_handle_hover="#585b70",
    button_background="#45475a",
    button_foreground="#cdd6f4",
    button_hover="#585b70",
    button_pressed="#313244",
    input_background="#313244",
    input_border="#45475a",
    input_focus_border="#89b4fa",
    success="#a6e3a1",
    warning="#f9e2af",
    error="#f38ba8",
    info="#89b4fa",
    scope_depth_colors=["#f9e2af", "#89b4fa", "#a6e3a1", "#cba6f7"],
)


LIGHT_UI_THEME = UITheme(
    name="Light",
    is_dark=False,
    background="#eff1f5",
    foreground="#4c4f69",
    accent="#1e66f5",
    accent_hover="#2a7afd",
    panel_background="#e6e9ef",
    panel_border="#bcc0cc",
    editor_background="#eff1f5",
    editor_foreground="#4c4f69",
    editor_line_highlight="#dce0e8",
    editor_selection="#acb0be",
    editor_gutter_bg="#e6e9ef",
    editor_gutter_fg="#8c8fa1",
    tab_background="#e6e9ef",
    tab_active_background="#eff1f5",
    tab_hover_background="#ccd0da",
    tab_border="#bcc0cc",
    browser_background="#e6e9ef",
    browser_item_hover="#ccd0da",
    browser_item_selected="#bcc0cc",
    terminal_background="#dce0e8",
    terminal_foreground="#4c4f69",
    scrollbar_background="#e6e9ef",
    scrollbar_handle="#bcc0cc",
    scrollbar_handle_hover="#acb0be",
    button_background="#bcc0cc",
    button_foreground="#4c4f69",
    button_hover="#acb0be",
    button_pressed="#ccd0da",
    input_background="#ccd0da",
    input_border="#bcc0cc",
    input_focus_border="#1e66f5",
    success="#40a02b",
    warning="#df8e1d",
    error="#d20f39",
    info="#1e66f5",
    scope_depth_colors=["#df8e1d", "#1e66f5", "#40a02b", "#8839ef"],
)


GREY_UI_THEME = UITheme(
    name="Grey",
    is_dark=False,
    background="#2b2d30",
    foreground="#bcbec4",
    accent="#4a9eff",
    accent_hover="#5eadff",
    panel_background="#25262a",
    panel_border="#3c3f41",
    editor_background="#2b2d30",
    editor_foreground="#bcbec4",
    editor_line_highlight="#323437",
    editor_selection="#214283",
    editor_gutter_bg="#25262a",
    editor_gutter_fg="#6c6f73",
    tab_background="#25262a",
    tab_active_background="#2b2d30",
    tab_hover_background="#3c3f41",
    tab_border="#3c3f41",
    browser_background="#25262a",
    browser_item_hover="#3c3f41",
    browser_item_selected="#4a5157",
    terminal_background="#1e1f22",
    terminal_foreground="#bcbec4",
    scrollbar_background="#25262a",
    scrollbar_handle="#4a5157",
    scrollbar_handle_hover="#5a5d62",
    button_background="#4a5157",
    button_foreground="#bcbec4",
    button_hover="#5a5d62",
    button_pressed="#3c3f41",
    input_background="#3c3f41",
    input_border="#4a5157",
    input_focus_border="#4a9eff",
    success="#6aab73",
    warning="#d5b778",
    error="#c75450",
    info="#4a9eff",
    scope_depth_colors=["#d5b778", "#4a9eff", "#6aab73", "#b58ee0"],
)


SOLARIZED_LIGHT_UI_THEME = UITheme(
    name="Solarized Light",
    is_dark=False,
    background="#fdf6e3",
    foreground="#657b83",
    accent="#268bd2",
    accent_hover="#2aa1f5",
    panel_background="#eee8d5",
    panel_border="#93a1a1",
    editor_background="#fdf6e3",
    editor_foreground="#657b83",
    editor_line_highlight="#eee8d5",
    editor_selection="#eee8d5",
    editor_gutter_bg="#eee8d5",
    editor_gutter_fg="#93a1a1",
    tab_background="#eee8d5",
    tab_active_background="#fdf6e3",
    tab_hover_background="#e3dcc3",
    tab_border="#93a1a1",
    browser_background="#eee8d5",
    browser_item_hover="#e3dcc3",
    browser_item_selected="#d9d2ba",
    terminal_background="#eee8d5",
    terminal_foreground="#657b83",
    scrollbar_background="#eee8d5",
    scrollbar_handle="#93a1a1",
    scrollbar_handle_hover="#839496",
    button_background="#93a1a1",
    button_foreground="#fdf6e3",
    button_hover="#839496",
    button_pressed="#e3dcc3",
    input_background="#eee8d5",
    input_border="#93a1a1",
    input_focus_border="#268bd2",
    success="#859900",
    warning="#b58900",
    error="#dc322f",
    info="#268bd2",
    scope_depth_colors=["#b58900", "#268bd2", "#859900", "#6c71c4"],
)


SOLARIZED_DARK_UI_THEME = UITheme(
    name="Solarized Dark",
    is_dark=True,
    background="#002b36",
    foreground="#839496",
    accent="#268bd2",
    accent_hover="#2aa1f5",
    panel_background="#00252f",
    panel_border="#073642",
    editor_background="#002b36",
    editor_foreground="#839496",
    editor_line_highlight="#073642",
    editor_selection="#073642",
    editor_gutter_bg="#00252f",
    editor_gutter_fg="#586e75",
    tab_background="#00252f",
    tab_active_background="#002b36",
    tab_hover_background="#073642",
    tab_border="#073642",
    browser_background="#00252f",
    browser_item_hover="#073642",
    browser_item_selected="#0a4a5c",
    terminal_background="#001e26",
    terminal_foreground="#839496",
    scrollbar_background="#00252f",
    scrollbar_handle="#073642",
    scrollbar_handle_hover="#0a4a5c",
    button_background="#073642",
    button_foreground="#839496",
    button_hover="#0a4a5c",
    button_pressed="#00252f",
    input_background="#073642",
    input_border="#586e75",
    input_focus_border="#268bd2",
    success="#859900",
    warning="#b58900",
    error="#dc322f",
    info="#268bd2",
    scope_depth_colors=["#b58900", "#268bd2", "#859900", "#6c71c4"],
)


HIGH_CONTRAST_UI_THEME = UITheme(
    name="High Contrast",
    is_dark=True,
    background="#000000",
    foreground="#ffffff",
    accent="#00d4ff",
    accent_hover="#00e4ff",
    panel_background="#0a0a0a",
    panel_border="#404040",
    editor_background="#000000",
    editor_foreground="#ffffff",
    editor_line_highlight="#1a1a1a",
    editor_selection="#264f78",
    editor_gutter_bg="#0a0a0a",
    editor_gutter_fg="#808080",
    tab_background="#0a0a0a",
    tab_active_background="#000000",
    tab_hover_background="#2a2a2a",
    tab_border="#404040",
    browser_background="#0a0a0a",
    browser_item_hover="#2a2a2a",
    browser_item_selected="#404040",
    terminal_background="#000000",
    terminal_foreground="#ffffff",
    scrollbar_background="#0a0a0a",
    scrollbar_handle="#404040",
    scrollbar_handle_hover="#606060",
    button_background="#404040",
    button_foreground="#ffffff",
    button_hover="#606060",
    button_pressed="#2a2a2a",
    input_background="#1a1a1a",
    input_border="#404040",
    input_focus_border="#00d4ff",
    success="#00ff00",
    warning="#ffff00",
    error="#ff0000",
    info="#00d4ff",
    scope_depth_colors=["#ffff00", "#00d4ff", "#00ff00", "#ff00ff"],
)


BUILTIN_UI_THEMES = {
    "dark": DARK_UI_THEME,
    "light": LIGHT_UI_THEME,
    "grey": GREY_UI_THEME,
    "solarized_light": SOLARIZED_LIGHT_UI_THEME,
    "solarized_dark": SOLARIZED_DARK_UI_THEME,
    "high_contrast": HIGH_CONTRAST_UI_THEME,
}


# Default syntax theme (fallback)
DEFAULT_SYNTAX_THEME = SyntaxColors(
    keyword="#cba6f7",
    builtin="#f9e2af",
    type="#94e2d5",
    string="#a6e3a1",
    number="#fab387",
    comment="#6c7086",
    operator="#94e2d5",
    function="#89b4fa",
    variable="#f38ba8",
    constant="#fab387",
    identifier="#cdd6f4",
    interpolation="#cba6f7"
)


class GeanyThemeParser:
    """Parser for Geany .conf theme files"""
    
    def __init__(self, conf_path: Path):
        self.conf_path = conf_path
        self.parser = ConfigParser()
        self.named_colors: Dict[str, str] = {}
        
    def parse(self) -> Optional[SyntaxColors]:
        """Parse a Geany .conf file and return SyntaxColors"""
        try:
            self.parser.read(self.conf_path, encoding='utf-8')
            
            # Parse named colors first
            if self.parser.has_section('named_colors'):
                for key, value in self.parser.items('named_colors'):
                    self.named_colors[key] = self._normalize_color(value)
            
            # Parse named styles
            if self.parser.has_section('named_styles'):
                return self._parse_syntax_colors()
            
            return None
            
        except Exception as e:
            print(f"Warning: Could not parse theme {self.conf_path}: {e}")
            return None
    
    def _normalize_color(self, color: str) -> str:
        """Normalize color format to #RRGGBB"""
        color = color.strip()
        
        # Handle hex colors
        if color.startswith('#'):
            if len(color) == 4:  # #RGB -> #RRGGBB
                return f"#{color[1]*2}{color[2]*2}{color[3]*2}"
            return color
        
        # Handle 0x format
        if color.startswith('0x'):
            return '#' + color[2:]
        
        # Handle named colors
        if color in self.named_colors:
            return self.named_colors[color]
        
        return color
    
    def _parse_style(self, style_str: str) -> Optional[str]:
        """Parse a Geany style string (foreground;background;bold;italic)"""
        parts = style_str.split(';')
        if parts and parts[0]:
            color = parts[0].strip()
            return self._resolve_color(color)
        return None
    
    def _resolve_color(self, color: str) -> str:
        """Resolve a color, checking named colors"""
        if color in self.named_colors:
            return self.named_colors[color]
        return self._normalize_color(color)
    
    def _get_color(self, section: str, key: str, default: str) -> str:
        """Get a color from the config, with fallback"""
        try:
            if self.parser.has_option(section, key):
                style = self.parser.get(section, key)
                parsed = self._parse_style(style)
                if parsed:
                    return parsed
        except Exception:
            pass
        return default
    
    def _parse_syntax_colors(self) -> SyntaxColors:
        """Parse syntax colors from Geany theme"""
        # Map Geany elements to our SyntaxColors
        return SyntaxColors(
            keyword=self._get_color('named_styles', 'keyword', DEFAULT_SYNTAX_THEME.keyword),
            builtin=self._get_color('named_styles', 'type', DEFAULT_SYNTAX_THEME.builtin),
            type=self._get_color('named_styles', 'class', DEFAULT_SYNTAX_THEME.type),
            string=self._get_color('named_styles', 'string', DEFAULT_SYNTAX_THEME.string),
            number=self._get_color('named_styles', 'number', DEFAULT_SYNTAX_THEME.number),
            comment=self._get_color('named_styles', 'comment', DEFAULT_SYNTAX_THEME.comment),
            operator=self._get_color('named_styles', 'operator', DEFAULT_SYNTAX_THEME.operator),
            function=self._get_color('named_styles', 'function', DEFAULT_SYNTAX_THEME.function),
            variable=self._get_color('named_styles', 'identifier', DEFAULT_SYNTAX_THEME.variable),
            constant=self._get_color('named_styles', 'number', DEFAULT_SYNTAX_THEME.constant),
            identifier=self._get_color('named_styles', 'default', DEFAULT_SYNTAX_THEME.identifier),
            interpolation=self._get_color('named_styles', 'keyword', DEFAULT_SYNTAX_THEME.interpolation),
        )


class ThemeManager:
    """Manages UI themes and syntax themes for the IDE"""

    def __init__(self, settings: SettingsManager):
        self.settings = settings
        self.ui_themes = dict(BUILTIN_UI_THEMES)
        # Default theme type needs to be set manually as it's not determined by parser
        DEFAULT_SYNTAX_THEME.theme_type = "dark" 
        self.syntax_themes: Dict[str, SyntaxColors] = {"default": DEFAULT_SYNTAX_THEME}
        self.syntax_themes_dir = self._get_syntax_themes_dir()
        self._current_ui_theme: Optional[UITheme] = None
        self._current_syntax_theme: Optional[SyntaxColors] = None
        
        # Copy bundled themes to user config directory on first run
        self._copy_bundled_themes()
        
        # Load syntax themes from directory
        self.load_syntax_themes()
        
        # Migrate old settings if needed
        self._migrate_old_settings()

    def _get_syntax_themes_dir(self) -> Path:
        """Get the user config syntax themes directory"""
        config_dir = get_config_dir() / "themes" / "syntax"
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir
    
    def _get_bundled_themes_dir(self) -> Path:
        """Get the bundled syntax themes directory from the package"""
        import sys

        # For compiled executable (PyInstaller)
        if getattr(sys, 'frozen', False):
            # PyInstaller 6.x: _MEIPASS points to the _internal/ directory where
            # all bundled data files are extracted (both onefile and onedir modes).
            if hasattr(sys, '_MEIPASS'):
                meipass_themes = Path(sys._MEIPASS) / "themes" / "syntax"
                if meipass_themes.exists():
                    return meipass_themes
            # Fallback: FHS-style system installation (exe_dir/share/themes/syntax)
            exe_dir = Path(sys.executable).parent
            share_themes = exe_dir / "share" / "themes" / "syntax"
            if share_themes.exists():
                return share_themes

        # For source distribution: use package directory
        return Path(__file__).parent.parent / "themes" / "syntax"
    
    def _copy_bundled_themes(self):
        """Copy bundled .conf themes to user config directory on first run"""
        bundled_dir = self._get_bundled_themes_dir()
        if not bundled_dir.exists():
            return
        
        user_dir = self.syntax_themes_dir
        
        # Copy all .conf files from bundled directory to user directory
        # Skip if file already exists (don't overwrite user modifications)
        for conf_file in bundled_dir.glob("*.conf"):
            dest_file = user_dir / conf_file.name
            if not dest_file.exists():
                try:
                    import shutil
                    shutil.copy2(conf_file, dest_file)
                    print(f"Copied bundled theme: {conf_file.name}")
                except Exception as e:
                    print(f"Warning: Could not copy bundled theme {conf_file.name}: {e}")

    def _migrate_old_settings(self):
        """Migrate old single-theme setting to dual theme settings"""
        # Check if old 'current_theme' exists but new settings don't
        if hasattr(self.settings.settings.theme, 'current_theme'):
            old_theme = self.settings.settings.theme.current_theme
            
            # If new settings don't exist, migrate
            if not hasattr(self.settings.settings.theme, 'ui_theme'):
                self.settings.settings.theme.ui_theme = old_theme
                self.settings.settings.theme.syntax_theme = "default"
                self.settings.save()

    def load_syntax_themes(self):
        """Load all .conf files from the syntax themes directory"""
        if not self.syntax_themes_dir.exists():
            return
        
        for conf_file in self.syntax_themes_dir.glob("*.conf"):
            try:
                parser = GeanyThemeParser(conf_file)
                syntax_colors = parser.parse()
                
                if syntax_colors:
                    # Determine theme type (dark/light compatibility)
                    syntax_colors.theme_type = _determine_theme_type(syntax_colors)
                    
                    # Use filename without extension as theme name
                    theme_name = conf_file.stem
                    self.syntax_themes[theme_name] = syntax_colors
                    print(f"Loaded syntax theme: {theme_name} ({syntax_colors.theme_type})")
                    
            except Exception as e:
                print(f"Warning: Could not load syntax theme {conf_file}: {e}")

    def get_compatible_syntax_themes(self, is_dark: bool) -> list:
        """Get list of syntax themes compatible with the given UI darkness"""
        compatible = []
        target_type = "dark" if is_dark else "light"
        
        for name, theme in self.syntax_themes.items():
            # Include if types match or if theme is 'any'
            if theme.theme_type == target_type or theme.theme_type == "any":
                compatible.append(name)
        
        return sorted(compatible)

    def get_ui_theme(self, name: str) -> UITheme:
        """Get a UI theme by name"""
        return self.ui_themes.get(name.lower(), DARK_UI_THEME)

    def get_syntax_theme(self, name: str) -> SyntaxColors:
        """Get a syntax theme by name"""
        return self.syntax_themes.get(name.lower(), DEFAULT_SYNTAX_THEME)

    def get_current_ui_theme(self) -> UITheme:
        """Get the currently active UI theme"""
        if self._current_ui_theme is None:
            ui_theme_name = getattr(self.settings.settings.theme, 'ui_theme', 'dark')
            self._current_ui_theme = self.get_ui_theme(ui_theme_name)
        return self._current_ui_theme

    def get_current_syntax_theme(self) -> SyntaxColors:
        """Get the currently active syntax theme"""
        if self._current_syntax_theme is None:
            syntax_theme_name = getattr(self.settings.settings.theme, 'syntax_theme', 'default')
            self._current_syntax_theme = self.get_syntax_theme(syntax_theme_name)
        return self._current_syntax_theme

    def set_ui_theme(self, name: str):
        """Set the current UI theme"""
        self.settings.settings.theme.ui_theme = name
        self._current_ui_theme = self.get_ui_theme(name)
        self.settings.save()

    def set_syntax_theme(self, name: str):
        """Set the current syntax theme"""
        self.settings.settings.theme.syntax_theme = name
        self._current_syntax_theme = self.get_syntax_theme(name)
        self.settings.save()

    def get_available_ui_themes(self) -> list:
        """Get list of available UI theme names"""
        return list(self.ui_themes.keys())

    def get_available_syntax_themes(self) -> list:
        """Get list of available syntax theme names"""
        return list(self.syntax_themes.keys())
    
    def get_current_theme(self):
        """Get current theme as a combined Theme object (for backward compatibility)"""
        # Import here to avoid circular dependency
        from forge_ide.app.themes import Theme
        ui_theme = self.get_current_ui_theme()
        syntax_theme = self.get_current_syntax_theme()
        
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


    def get_current_stylesheet(self) -> str:
        """Generate Qt stylesheet for current UI theme"""
        return self.generate_stylesheet(self.get_current_ui_theme())

    def generate_stylesheet(self, theme: UITheme) -> str:
        """Generate a complete Qt stylesheet from a UI theme"""
        return f"""
/* Main Window */
QMainWindow, QWidget {{
    background-color: {theme.background};
    color: {theme.foreground};
    font-family: "Segoe UI", "SF Pro Display", "Ubuntu", sans-serif;
    font-size: 13px;
}}

/* Menu Bar */
QMenuBar {{
    background-color: {theme.panel_background};
    color: {theme.foreground};
    border-bottom: 1px solid {theme.panel_border};
    padding: 4px 0px;
}}

QMenuBar::item {{
    background: transparent;
    padding: 6px 12px;
    border-radius: 4px;
    margin: 0 2px;
}}

QMenuBar::item:selected {{
    background-color: {theme.button_hover};
}}

/* Menus */
QMenu {{
    background-color: {theme.panel_background};
    color: {theme.foreground};
    border: 1px solid {theme.panel_border};
    border-radius: 8px;
    padding: 6px;
}}

QMenu::item {{
    padding: 8px 32px 8px 24px;
    border-radius: 4px;
}}

QMenu::item:selected {{
    background-color: {theme.accent};
    color: {theme.background};
}}

/* Toolbar */
QToolBar {{
    background-color: {theme.panel_background};
    border: none;
    padding: 4px;
    spacing: 4px;
}}

QToolButton {{
    background-color: transparent;
    border: none;
    border-radius: 6px;
    padding: 8px;
    margin: 2px;
}}

QToolButton:hover {{
    background-color: {theme.button_hover};
}}

/* Tabs */
QTabWidget::pane {{
    border: none;
    background-color: {theme.editor_background};
}}

QTabBar::tab {{
    background-color: {theme.tab_background};
    color: {theme.foreground};
    border: none;
    border-bottom: 2px solid transparent;
    padding: 10px 20px;
}}

QTabBar::tab:selected {{
    background-color: {theme.tab_active_background};
    border-bottom: 2px solid {theme.accent};
}}

QTabBar::tab:hover:!selected {{
    background-color: {theme.tab_hover_background};
}}

/* Splitter */
QSplitter::handle {{
    background-color: {theme.panel_border};
}}

QSplitter::handle:horizontal {{
    width: 2px;
}}

QSplitter::handle:vertical {{
    height: 2px;
}}

/* Scrollbars */
QScrollBar:vertical {{
    background: {theme.scrollbar_background};
    width: 12px;
    border-radius: 6px;
}}

QScrollBar::handle:vertical {{
    background: {theme.scrollbar_handle};
    border-radius: 5px;
    min-height: 30px;
    margin: 2px;
}}

QScrollBar::handle:vertical:hover {{
    background: {theme.scrollbar_handle_hover};
}}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0;
}}

QScrollBar:horizontal {{
    background: {theme.scrollbar_background};
    height: 12px;
    border-radius: 6px;
}}

QScrollBar::handle:horizontal {{
    background: {theme.scrollbar_handle};
    border-radius: 5px;
    min-width: 30px;
    margin: 2px;
}}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
    width: 0;
}}

/* Tree View (File Browser) */
QTreeView {{
    background-color: {theme.browser_background};
    color: {theme.foreground};
    border: none;
    padding: 4px;
}}

QTreeView::item {{
    padding: 6px 8px;
    border-radius: 4px;
}}

QTreeView::item:hover {{
    background-color: {theme.browser_item_hover};
}}

QTreeView::item:selected {{
    background-color: {theme.browser_item_selected};
}}

/* Push Button */
QPushButton {{
    background-color: {theme.button_background};
    color: {theme.button_foreground};
    border: none;
    border-radius: 6px;
    padding: 8px 16px;
}}

QPushButton:hover {{
    background-color: {theme.button_hover};
}}

/* Line Edit */
QLineEdit {{
    background-color: {theme.input_background};
    color: {theme.foreground};
    border: 1px solid {theme.input_border};
    border-radius: 6px;
    padding: 8px 12px;
}}

QLineEdit:focus {{
    border-color: {theme.input_focus_border};
}}

/* Status Bar */
QStatusBar {{
    background-color: {theme.panel_background};
    color: {theme.foreground};
    border-top: 1px solid {theme.panel_border};
    padding: 4px;
}}
"""

